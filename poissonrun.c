#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <sys/wait.h>

static int random_fd = -1;

static double get_random(void)
{
  ssize_t ret;
  uint8_t buf[4];
  size_t rb;

  if (random_fd == -1) {
    random_fd = open("/dev/urandom", O_RDONLY);
    if (random_fd < 0) {
      fprintf(stderr, "no urandom: %s\n", strerror(errno));
      exit(-1);
    }
  }

  rb = 0;
  while (rb < sizeof buf) {
    ret = read(random_fd, buf, sizeof buf);
    if (ret < 0) {
      if (errno == EINTR || errno == EAGAIN)
	continue;
      fprintf(stderr, "error on reading urandom: %s\n", strerror(errno));
      exit(-1);
    } else if (ret == 0) {
      fprintf(stderr, "unexpected eof on urandom\n");
      exit(-1);
    }
    rb += ret;
  }

  return ((double) 1.0) * ((* (uint32_t * ) buf) & 0x3fffffff) / 0x40000000;
}


int main(int argc, char **argv)
{
  long long i;
  double interval;

  if (argc < 3) {
    fprintf(stderr, "Usage: %s timeinterval command args...\n", argv[0]);
    return -1;
  }

  interval = atof(argv[1]);
  if (interval <= 0.0) {
    fprintf(stderr, "Must have a positive time interval value.\n");
    return -1;
  }

  for (i = 0;; i++) {
    double r = get_random();
    if (r < (1 / interval)) {
      int rv;
      fprintf(stderr, "%lld: now\n", i);
      rv = fork();
      if (rv == 0) {
	execvp(argv[2], &argv[2]);
	abort();
      } else if (rv < 0) {
	fprintf(stderr, "Not able to fork.\n");
      } else {
	while (1) {
	  int ex = wait(NULL);
	  if (ex == rv)
	    break;
	  if (ex < 0 && errno == EINTR)
	    continue;
	  fprintf(stderr, "A bug happened. ex = %d err: %s\n", ex, strerror(errno));
	  break;
	}
      }

    }
    sleep(1);
  }
}
