// Send and receive APRS "messages" for Ham Radio of Things (HRoT)
//
// Incoming messages are put into separate files in the receive queue directory,
// typically /dev/shm/RQ.   This is a RAM disk so it is faster and will reduce
// wear and tear on your SD memory card if using a Raspberry Pi, etc.
//
// "kissutil," from direwolf can be used to take frames from a KISS TNC and put
// them in the proper format.
//
// For transmitting, files are created in the transmit queue directory, typically
// /dev/shm/TQ.  kissutil can be used to read those files, in standard monitoring
// format, and command a KISS TNC to transmit them.

// John Langner, WB2OSZ, Feb. 2019


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>

#include <mosquitto.h>

#include "hrot.h"
#include "subscribe.h"
#include "aprsmsg.h"

// Called periodically (e.g. once a second) to look for files in the receive queue.
//
// If it is a message, addressed to me, in the proper format, process it.

static void amsg_process (struct hrot_s *h, char *stuff);

void amsg_poll (struct hrot_s *h) 
{
	DIR *dp;
	struct dirent *ep;

// If I was overly ambitious, the file names could be sorted so they could be
// processed in some predictable order.    (scandir instead.)

	dp = opendir (h->rq_dir);
	if (dp != NULL) {
	  while ((ep = readdir(dp)) != NULL) {
	    char path [300];
	    FILE *fp;
	    if (ep->d_name[0] == '.') {
	      continue;
	    }
	    printf ("---\nProcessing %s from receive queue...\n", ep->d_name);
	    strcpy (path, h->rq_dir);
	    strcat (path, "/");
	    strcat (path, ep->d_name);
	    fp = fopen (path, "r");
	    if (fp != NULL) {
	      char stuff[512];
	      while (fgets(stuff, sizeof(stuff), fp) != NULL) {
	        char *p;
	        p = stuff + strlen(stuff) - 1;
	        if (p >= stuff && *p == '\n') {
	          *p = '\0';
	        }
	        printf ("%s\n", stuff);
	        amsg_process (h, stuff);		// Process each line of file.
	      }
	      fclose (fp);
	      unlink (path);				// Remove file after processing.
	    }
	    else {
	      printf("Can't open for read: %s\n", path);
	    }
	  }
	  closedir (dp);
	}
	else {
	  printf ("Could not list receive queue directory %s\n", h->rq_dir);
	}
}

// This is called for each line of a file in the receive queue.
//
// First, any third party header must be removed.
// Then test to see if it is a message addressed to me.
// Process any HRoT formatted messages like these.
//
//	src>dst,digi::addressee:PUB:topic=value
//	src>dst,digi::addressee:SUB:topic
//	src>dst,digi::addressee:UNS:
//	src>dst,digi::addressee:UNS:topic
//
// Any authentication signature and message ids should have been removed earlier.
//


static void amsg_process (struct hrot_s *h, char *amsg) 
{
	int chan = 0;			// Radio channel.
	char src[12] = "";		// Who did it come from?  CALL-SSID
	char dst[12] = "";		// Don't care.  Usually product ID.
	char addr[12] = "";		// Addressed to whom?  Me?
	char cmd[4] = "";		// Command such as pub, sub, or top.  Case insensitive.
	char topic[256] = "";		// For either subscribe or publish.
	char payload[256] = "";		// Only for publish.
	int qos = 0;
	bool retain = false;
	int e;
	char *p = amsg;

// Optionally preceded by channel number inside [].

	if (*p == '[') {
	  p++;
	  chan = atoi(p);
	  while (isdigit(*p)) p++;
	  if (*p != ']') {
	    printf ("Parse error: Expected \"]\" after channel number.\n");
	    return;
	  }
	  p++;
	  while (*p == ' ') p++;
	}

// Extract src, source address.
	
	char *r = src;
	while ((isupper(*p) || isdigit(*p) || *p == '-') && strlen(src) < 9) {
	  *r++ = *p++;
	  *r = '\0';
	}
	if (*p != '>') {
	  printf ("Parse error: Expected \">\" after \"%s\"\n", src);
	  return;
	}
	p++;

// Extract dst, destination.  Usually the product id.

	r = dst;
	while ((isupper(*p) || isdigit(*p) || *p == '-') && strlen(dst) < 9) {
	  *r++ = *p++;
	  *r = '\0';
	}
	if (*p != ',' && *p != ':') {
	  printf ("Parse error: Expected \",\" or \":\" after \"%s\"\n", dst);
	  return;
	}

// We probably don't care about the via path.

	while (*p != ':' && *p != '\0') {
	  p++;
	}
	p++;

// Should now point at Data Type Indicator.
// If third party traffic, process without the wrapper.

	if (*p == '}') {
	  amsg_process (h, p+1);
	  return;
	}
	  
	if (*p != ':') {
	  printf ("Ignore - Not an APRS style \"message.\"\n");
	  return;		// Not a "message."
	}
	p++;

// Addressee should be exactly 9 characters followed by ":".
// Trim trailing spaces.

	if (p[9] != ':') {
	  printf ("Parse Error: Addressee must be exactly 9 characters followed by \":\"\n");
	  return;
	}

	memcpy (addr, p, 9);
	addr[9] = '\0';
	int i;
	for (i = 8; i >= 0; i--) {
	  if (addr[i] == ' ')
	    addr[i] = '\0';
	  else
	    break;
	}
	p += 10;

// Is this addressed to me?

	if (strcmp(addr, h->mycall) != 0) {
	  printf ("Ignore - Addressed to '%s' not to my name '%s'\n", addr, h->mycall);
	  return;
	}

// Get command.  Case insensitive.
// TODO: Rethink this. Might want to add options after the 3 letters.

	if (p[3] != ':') {
	  printf ("Parse Error: Expected 3 character command followed by \":\"\n");
	  return;
	}
	memcpy (cmd, p, 3);
	cmd[3] = '\0';
	p += 4;

// Get any topic and payload.

	char *t = topic;
	while (*p != '=' && *p != '\0') {
	  *t++ = *p++;
	}
	*t = '\0';

	if (*p == '=') {
	  strcpy (payload, p+1);
	}
	   
// Publish message?
// This needs to have topic and a value (payload).

	if (strcasecmp(cmd, "pub") == 0) {
 	  printf ("Publish %s=%s from %s\n", topic, payload, src);	

	  e = mosquitto_pub_topic_check (topic);
	  if (e != MOSQ_ERR_SUCCESS) {
	    // _strerror returns "Invalid function arguments provided."
	    // Not very informative so use our own.
	    amsg_send (h, chan, h->mycall, PRODUCT_ID, h->via, src, "Invalid topic name for publish.");
	    return;
	  } 

	  e = mosquitto_publish(h->mosq, NULL, topic, strlen(payload), payload, qos, retain);
	  if (e != MOSQ_ERR_SUCCESS) {
	    amsg_send (h, chan, h->mycall, PRODUCT_ID, h->via, src, mosquitto_strerror(e));
	    return;
	  } 

	  (void) client_add (chan, src);
	  return;
	}

// Subscribe to topic?
// This must have a topic.  + and # are allowed for limited wildcarding.

	if (strcasecmp(cmd, "sub") == 0) {
	  printf ("Subscribe %s to %s\n", src, topic);	

	  e = mosquitto_sub_topic_check (topic);
	  if (e != MOSQ_ERR_SUCCESS) {
	    amsg_send (h, chan, h->mycall, PRODUCT_ID, h->via, src, "Invalid topic for subscribe.");
	    return;
	  } 

	  // Add to our own table so we can send a subset of what we get from Mosquitto.

	  int cid = client_add (chan, src);
	  subscr_add (cid, topic);
	  return;
	}

// Unsubscribe from topic?
// If no topic listed, unsubscribe from all for this client.

	if (strcasecmp(cmd, "uns") == 0) {
	  printf ("Unsubscribe\n");	

	  if (strlen(topic) > 0) {
	    e = mosquitto_sub_topic_check (topic);
	    if (e != MOSQ_ERR_SUCCESS) {
	      amsg_send (h, chan, h->mycall, PRODUCT_ID, h->via, src, "Invalid topic for unsubscribe.");
	      return;
	    } 
	  }

	  int cid = client_add (chan, src);
	  subscr_remove (cid, topic);
	  return;
	}

	char reply[80];
	snprintf (reply, sizeof(reply), "Invalid command \"%s\"\n", cmd);	
	amsg_send (h, chan, h->mycall, PRODUCT_ID, h->via, src, reply);

}


// Send message over the radio.
// More accurately, put in transmit queue where someone else will pick it up and transmit.

void amsg_send (struct hrot_s *h, int chan, char *src, char *dst, char *via, char *addressee, const char *msgbody)
{
	char packet[512];
	char filename[60];
	char fullpath[300];
	struct timeval tv;


	snprintf (packet, sizeof(packet), "[%d] %s>%s%s%s::%-9s:%s",
		chan,
		src, dst, strlen(via) ? "," : "", via,
		addressee, msgbody);

	printf ("Send: %s\n", packet);

// Get unique name by taking current time to microsecond resolution.
// (Even on a Raspberry Pi, these can come out less than a millisecond apart.)
// There are certainly other approaches that could be taken.

	gettimeofday (&tv, NULL);

	snprintf (filename, sizeof(filename), "%d.%06d", (int)tv.tv_sec, (int)tv.tv_usec);

	strcpy (fullpath, h->tq_dir);
	strcat (fullpath, "/");
	strcat (fullpath, filename);

	FILE *fp = fopen (fullpath, "w");
	if (fp != NULL) {
	  fprintf (fp, "%s\n", packet);
	  fclose (fp);
	}
	else {
	  printf ("Could not open %s for write.\n", fullpath);
	}
}

// end aprsmsg.c

