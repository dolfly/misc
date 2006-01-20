/* Poissonrun. No copyrights claimed.
   Contact Heikki Orsila <heikki.orsila@iki.fi> for anything. */

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


static void print_help(void)
{
  printf("poissonrun by Heikki Orsila <heikki.orsila@iki.fi>. No copyrights claimed.\n");
  printf("\n");
  printf("The program runs a given command statistically every T seconds. The interval T\n");
  printf("is given on the command line:\n");
  printf("\n");
  printf(" $ poissonrun T command arg1 arg2 ...\n");
  printf("\n");
  printf("Randomization happens once every second and based on that it is decided\n");
  printf("if the command should be run. Time is not counted when the command is being\n");
  printf("run. Time interval T should be big compared to command\n");
  printf("execution time so that this process is statistically reasonable.\n");
  printf("Time interval T should be given as a floating-point number bigger than 1.\n");
  printf("\n");
  printf("The program needs /dev/urandom to work.\n");
  printf("\n");
  printf("Example 1: Play a wav file statistically every 64 seconds. Danger: irritating.\n");
  printf("\tpoissonrun 64 aplay foo.wav\n");
}


int main(int argc, char **argv)
{
  long long i;
  double interval;

  if ((argc == 2 && strcmp(argv[1], "-h") == 0) ||
      (argc == 2 && strcmp(argv[1], "--help") == 0)) {
    print_help();
    return 0;
  }

  if (argc < 3) {
    fprintf(stderr, "Not enough args. 2 is a minimum.\n");
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
