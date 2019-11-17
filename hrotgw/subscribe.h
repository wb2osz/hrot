

int client_find (int chan, char *addr);
int client_add (int chan, char *addr);

int subscr_find (int cid, char *pattern);
int subscr_add (int cid, char *pattern);
void subscr_remove (int cid, char *pattern);
void subscr_sendto (struct hrot_s *h, char *topic, char *payload);

