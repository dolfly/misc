#ifndef _NETAUDIO_NET_H_
#define _NETAUDIO_NET_H_

int net_listen(char *hostname, char *port, char *protocol);
int net_open(char *hostname, char *port, char *protocol);

#endif
