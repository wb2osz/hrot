// Ham Radio of Things (HRoT) to MQTT gateway.
//
// This relays IoT type messages between Ham Radio and a popular MQTT broker.
//
// For receiving, we look for files in the receive queue, typically /dev/shm/RQ.
// If these are the right format, they are used to publish to and subscribe to topics.
//
// In the other direction, APRS messages are put in the transmit queue,
// typically /dev/shm/TQ, where they get picked up and transmitted.
//
// See accompanying documentation for more indepth discussion.
//
// John Langner, WB2OSZ, Feb. 2019


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <getopt.h>

// Library API here:  https://mosquitto.org/api/files/mosquitto-h.html

#include <mosquitto.h>

#include "hrot.h"
#include "subscribe.h"
#include "aprsmsg.h"


static struct hrot_s h;		// Command line options and other global stuff.


// This could be useful for low level debugging,
// Normally nothing comes out.

void on_log (struct mosquitto *mosq, void *userdata, int level, const char *str)
{
	switch(level){
	  case MOSQ_LOG_INFO:
	  case MOSQ_LOG_NOTICE:
	  case MOSQ_LOG_WARNING:
	  case MOSQ_LOG_ERR:
	    printf ("Mosquitto log: %i %s\n", level, str);
	    break;
	  //case MOSQ_LOG_DEBUG:
	  default:
	    break;
	}
}  


// This is called when something is published to mosquitto.
// We relay the message to anyone subscribed to the topic.

void on_message(struct mosquitto *mosq, void *obj, const struct mosquitto_message *message)
{
	// Don't know if we are guaranteed to have nul terminator after
	// payload because binary data is allowed.
	// Copy to a local variable to play it safe.

	char str[message->payloadlen + 1];
	memcpy (str, message->payload, message->payloadlen);
	str[message->payloadlen] = '\0';

	printf  ("---\nMessage received from MQTT Broker: %s=%s\n", message->topic, str);
	subscr_sendto (&h, message->topic, str);
}


void mqtt_setup(){

	h.keepalive = 60;
	h.clean_session = true;
	h.mosq = NULL;
  
	mosquitto_lib_init();		// Must be called before any other functions.

	h.mosq = mosquitto_new (NULL, h.clean_session, NULL);
	if (!h.mosq){
	  fprintf(stderr, "Error: Out of memory or invalid parameters.\n");
	  exit(EXIT_FAILURE);
	}
  
	mosquitto_log_callback_set(h.mosq, on_log);
	mosquitto_message_callback_set(h.mosq, on_message);
 
// Connect to the Mosquitto server.
 
	if(mosquitto_connect(h.mosq, h.host, h.port, h.keepalive)){
	  fprintf(stderr, "Unable to connect to MQTT Server '%s'.\n", h.host);
	  exit(EXIT_FAILURE);
	}

// Start a new thread to process network traffic so we don't have to poll for it..

	int loop = mosquitto_loop_start(h.mosq);
	if(loop != MOSQ_ERR_SUCCESS){
	  fprintf(stderr, "Unable to start loop: %i\n", loop);
	  exit(EXIT_FAILURE);
	}
}

// Main.


int main (int argc, char *argv[])
{

// Command line arguments.

	strcpy (h.host, "localhost");		// -b	broker host
	h.port = 1883;
	strcpy (h.mycall, "N0CALL");		// -m 	mycall
	strcpy (h.via, "WIDE1-1");		// -v	via for sending
	strcpy (h.rq_dir, "/dev/shm/RQ");	// -r	receive queue
	strcpy (h.tq_dir, "/dev/shm/TQ");	// -t	transmit queue
						// -d		should have some kind of debug out.
	int opt;

	while ((opt = getopt (argc, argv, "b:m:v:r:t:")) != -1) {
	  switch (opt) {
	    case 'b':
	      strcpy (h.host, optarg);
	      break;
	    case 'm':
	      strcpy (h.mycall, optarg);	// TODO: overflow checking
						// TODO: must be upper case
	      break;
	    case 'v':
	      strcpy (h.via, optarg);
	      break;
	    case 'r':
	      strcpy (h.rq_dir, optarg);
	      break;
	    case 't':
	      strcpy (h.tq_dir, optarg);
	      break;
	    default:
	      // usage();			// TODO:  provide help.
	      break;
	  }
	}

// TODO: complain about any unused command line arguments.


// Connect to Mosquitto.

	mqtt_setup();

// We will simply subscribe to everything rather than trying to maintain
// the union of all the subscriptions from our clients.
// The expection is that the server will be near and the amount of traffic low.

	int r = mosquitto_subscribe(h.mosq,NULL,"#",0);
	if (r != MOSQ_ERR_SUCCESS) {
	  fprintf (stderr, "Subscribe failure.  Not much point in going on.\n");
	  exit (EXIT_FAILURE);
	}


// Check the receive queue directory periodically.
// Incoming from MQTT will arrive via the on_* functions.

	while(1) {
	  sleep(1);
	  amsg_poll (&h);
	}

// Currently unreachable.

	mosquitto_disconnect(h.mosq);
	h.mosq = NULL;
	exit (EXIT_SUCCESS);
}

// end hrotgw.c


