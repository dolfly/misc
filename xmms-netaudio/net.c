/* See xmms-netaudio copyrights.

This module has been heavily influenced by Richard W. Stevens networking
example code.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "net.h"

int
net_listen(char *hostname, char *port, char *protocol)
{
  int ret;
  int listenfd;
  struct addrinfo *res = 0;
  struct addrinfo *ressave;
  struct addrinfo hints;
  struct protoent *pe;

  if(!(pe = getprotobyname(protocol))) {
    fprintf(stderr, "net_listen: can't get protocol number\n");
    exit(-1);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = AI_PASSIVE;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = pe->p_proto;

  ret = getaddrinfo(hostname, port, &hints, &res);
  if(ret) {
    fprintf(stderr, "getaddrinfo: %s (%s:%s)\n", gai_strerror(ret), hostname, port);
    exit(-1);
  }
  ressave = res;

  listenfd = -1;
  do {
    listenfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(listenfd < 0)
      continue;             /* error, try next one */

    if(bind(listenfd, res->ai_addr, res->ai_addrlen) == 0)
      break;                /* success */
    close(listenfd);
  } while ((res = res->ai_next) != 0);

  if(!res) {
    /* fprintf(stderr, "netdd: tcp_listen error for %s:%s\n", hostname, port); */
    listenfd = -1;
  }
  freeaddrinfo(ressave);

  if(listenfd >= 0)
    listen(listenfd, 5);

  return listenfd;
}


int
net_open(char *hostname, char *port, char *protocol)
{
  int ret;
  int sockfd;
  struct addrinfo *res = 0;
  struct addrinfo *ressave;
  struct addrinfo hints;
  struct protoent *pe;

  if(!(pe = getprotobyname(protocol))) {
    fprintf(stderr, "net_open: can't get protocol number\n");
    exit(-1);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_flags = 0;
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = pe->p_proto;

  ret = getaddrinfo(hostname, port, &hints, &res);
  if(ret) {
    fprintf(stderr, "getaddrinfo: %s (%s:%s)\n", gai_strerror(ret), hostname, port);
    exit(-1);
  }
  ressave = res;

  sockfd = -1;
  do {
    sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(sockfd < 0)
      continue;
    if(connect(sockfd, res->ai_addr, res->ai_addrlen) == 0)
      break;
    close(sockfd);
  } while ((res = res->ai_next) != 0);

  if(!res) {
    fprintf(stderr, "tcp_connect error for %s:%s\n", hostname, port);
    sockfd = -1;
  }
  freeaddrinfo(ressave);
  return sockfd;
}
