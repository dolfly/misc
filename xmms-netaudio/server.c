#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>

#include "net.h"
#include "meta.h"

extern int errno;

static const int dspbufsize = 524288;
static char *dspbuf = 0;

static int stream_input(int fd) {
  return 0;
}

static int dsp_output(int fd) {
  return 0;
}

int main(int argc, char **argv) {
  int i;
  char *port = 0;
  struct pollfd pfd[3];
  int is_connected;
  int nfds;
  int ret;
  int streamfd, dspfd;

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

  dspbuf = malloc(dspbufsize);
  if (!dspbuf) {
    fprintf(stderr, "xmms-netaudio: not enough memory\n");
  }

  pfd[0].fd = net_listen(0, port, "tcp");
  pfd[0].events = POLLIN;

  is_connected = 0;
  streamfd = -1;
  dspfd = -1;

  while(1) {
    nfds = 1;
    if (streamfd >= 0) {
      pfd[nfds].fd = streamfd;
      pfd[nfds].events = POLLIN;
      nfds++;
    }
    if (dspfd >= 0) {
      /* this should be put on pfd only if there's data to write */
      pfd[nfds].fd = dspfd;
      pfd[nfds].events = POLLOUT;
      nfds++;
    }

    ret = poll(pfd, nfds, -1);
    if (ret == 0) {
      fprintf(stderr, "xmms-netaudio: interesting, poll returned zero\n");
    } else if (ret < 0) {
      if (errno != EINTR) {
	perror("xmms-netaudio: poll error");
	break;
      }
    }
    if (pfd[0].revents & POLLIN) {
      streamfd = accept(pfd[0].fd, 0, 0);
      if (streamfd < 0) {
	perror("xmms-netaudio: accept error");
	exit(-1);
      }
      pfd[1].fd = streamfd;
      pfd[1].events = POLLOUT;
    }

    for (i = 1; i < nfds; i++) {
      if (pfd[i].fd == streamfd) {
	(void) stream_input(streamfd);
      } else if (pfd[i].fd == dspfd) {
	(void) dsp_output(dspfd);
      }
    }
  }
  return 0;
}
