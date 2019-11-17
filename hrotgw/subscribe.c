// This is used to keep track of the clients and subscriptions over HRoT.

// John Langner, WB2OSZ, Feb. 2019

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

// Library API here:  https://mosquitto.org/api/files/mosquitto-h.html

#include <mosquitto.h>

#include "hrot.h"
#include "subscribe.h"
#include "aprsmsg.h"


// Keep it simple for now.  Later allocate dynamically or even
// use SQL database for persistence across restarts.

#define MAX_CLIENT 100

static struct {
	bool valid;	// Is table entry in use?
	int chan;	// Radio channel where found.
	char addr[12];	// callsign-ssid
	time_t last_heard;
} client[MAX_CLIENT];

#define MAX_SUBSCR (MAX_CLIENT*5/4)	// One client can have multiple subscriptions.

static struct {
	bool valid;	// Is table entry in use?
	int client_id;	// Index into client table.
	char *pattern;	// Dynamically allocated.
} subscr[MAX_SUBSCR];


// Return client id for given radio channel and station address.
// -1 if not found.

int client_find (int chan, char *addr)
{
	int n;

	for (n = 0; n < MAX_CLIENT; n++) {
	  if (client[n].valid && client[n].chan == chan && strcmp(client[n].addr, addr) == 0) {
	    client[n].last_heard = time(NULL);
	    return (n);
	  }
	}
	return (-1);		// not found 
}


// Add to client table if not there already.
// Either way, return the client id.

int client_add (int chan, char *addr)
{
	int n;

	n = client_find (chan, addr);
	if (n >= 0) {
	  return (n);
	}

	for (n = 0; n < MAX_SUBSCR; n++) {
	  if ( ! client[n].valid) {
	    client[n].chan = chan;
	    strcpy(client[n].addr, addr);
	    client[n].last_heard = time(NULL);
	    client[n].valid = true;
	    return (n);
	  }
	}
	return (-1);		// table full
}


// Return subscription id for given client and topic.
// -1 if not found.

int subscr_find (int cid, char *pattern)
{
	int n;

	for (n = 0; n < MAX_CLIENT; n++) {
	  if (subscr[n].valid && subscr[n].client_id == cid && strcmp(subscr[n].pattern, pattern) == 0) {
	    return (n);
	  }
	}
	return (-1);		// not found 
}


// Add to our subscription table, given a client id and subscription pattern.

int subscr_add (int cid, char *pattern) 
{
	int n;

	if (cid < 0) return (-1);

	n = subscr_find (cid, pattern);
	if (n >= 0) {
	  return (n);
	}

	for (n = 0; n < MAX_SUBSCR; n++) {
	  if ( ! subscr[n].valid) {
	    subscr[n].client_id = cid;
	    subscr[n].pattern = strdup(pattern);
	    subscr[n].valid = true;
	    return (n);
	  }
	}
	return (-1);		// table full
}


// Remove specified subscription pattern for given client id.
// Remove all for client if pattern is empty.


void subscr_remove (int cid, char *pattern) 
{
	int n;

	if (cid < 0) return;

	for (n = 0; n < MAX_SUBSCR; n++) {
	  if (subscr[n].valid && subscr[n].client_id == cid) {
	    if (strlen(pattern) == 0 || strcmp(subscr[n].pattern, pattern) == 0) {
	      char *p = subscr[n].pattern;
	      subscr[n].valid = false;
	      subscr[n].client_id = -1;
	      subscr[n].pattern = NULL;
	      free (p);
	    }
	  }
	}
}

// Display tables for troubleshooting.

void print_tables ()
{
	printf ("\n");
	printf ("id   chan  client addr  last heard\n");
	printf ("--   ----  -----------  ----------\n");

	for (int i = 0; i < MAX_CLIENT; i++) {
	  if (client[i].valid) {
	    printf ("%2d  %2d  %-9s  tbd...\n", i, client[i].chan, client[i].addr);
	  }
	}

	printf ("\n");
	printf ("id   cli  subscription\n");
	printf ("--   ---  ------------\n");

	for (int i = 0; i < MAX_SUBSCR; i++) {
	  if (subscr[i].valid) {
	    printf ("%2d  %2d  %s\n", i, subscr[i].client_id, subscr[i].pattern);
	  }
	}
	printf ("\n");
}


// Mosquitto has sent forwarded something that was published.
// Send it to all matching subscriptions.

void subscr_sendto (struct hrot_s *h, char *topic, char *payload)
{
	// Could get duplicates if multiple overlapping subscription patterns.
	// If we wanted to be more accurate, we could keep track of which client(s)
	// were sent this particular message and avoid duplicates.  Later.  Maybe.

	for (int n = 0; n < MAX_SUBSCR; n++) {
	  if (subscr[n].valid) {
	    bool result;
	    if (mosquitto_topic_matches_sub(subscr[n].pattern, topic, &result) == MOSQ_ERR_SUCCESS) {
	      if (result) {
	        int c = subscr[n].client_id;
	        assert (c >= 0 && c < MAX_CLIENT && client[c].valid);

	        char body[256];
	        snprintf (body, sizeof(body), "top:%s=%s", topic, payload);
	        amsg_send (h, client[c].chan, h->mycall, PRODUCT_ID, h->via, client[c].addr, body);
	      }
	    }
	  }
	}
}

// end subscribe.c

