// Command line options and other global stuff.

#define PRODUCT_ID "APMQ01"		// tocalls.txt

struct hrot_s {

// AX.25 parameters.

	char mycall[12];	// My call, optional -ssid
	char via[80];		// e.g.  "WIDE1-1,WIDE2-1"

// T/R queue directories.

	char rq_dir[80];	// Receive queue directory.
	char tq_dir[80];	// Transmit queue directory.

// Connection to Mosquitto.

	char host[40];
	int port;
	int keepalive;
	bool clean_session;
	struct mosquitto *mosq;
};

