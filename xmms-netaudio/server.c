#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>

#include "net.h"
#include "meta.h"

extern int errno;

int main(int argc, char **argv) {
  int i;
  char *port = 0;
  struct pollfd pfd[2];
  int is_connected;
  int n_streams;
  int nfds;
  int ret;

  if (argc < 3) {
    fprintf(stderr, "xmms-netaudio: not enough parameters. give a port to listen.\n");
    exit(-1);
  }

  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-p")) {
      if ((i + 1) >= argc)
	goto perr;
      port = strdup(argv[i+1]);
      if (!port) {
	fprintf(stderr, "xmms-netaudio: not enough memory\n");
	exit(-1);
      }
      i += 2;
      continue;
    }
  perr:
    fprintf(stderr, "xmms-netaudio: parameter error\n");
    exit(-1);
  }

  if (!port) {
    fprintf(stderr, "xmms-netaudio: port has not been given\n");
    exit(-1);
  }

  pfd[0].fd = net_listen(0, port, "tcp");
  pfd[0].events = POLLIN;
  nfds = 1;

  is_connected = 0;
  n_streams = 0;

  while(1) {
    ret = poll(pfd, nfds, -1);
    
  }
  return 0;
}
