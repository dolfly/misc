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
#include <sys/time.h>
#include <math.h>
#include <assert.h>

#include "version.h"

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
  printf("poissonrun %s by Heikki Orsila <heikki.orsila@iki.fi>. No copyrights claimed.\n", VERSION);
  printf("\n");
  printf("The program runs a given command statistically every T seconds. The command may\n");
  printf("run at any second but on average it will be run once every T seconds.\n");
  printf("Randomization happens once every second and based on that it is decided\n");
  printf("if the command should be run. Time is not counted when the command is being\n");
  printf("run. Time interval T should be big compared to command\n");
  printf("execution time so that this process is statistically reasonable.\n");
  printf("Time interval T should be given as a floating-point number bigger than 1.\n");
  printf("\n");
  printf(" USAGE:\n");
  printf("\tpoissonrun [-h] [-q] [-s] T command arg1 arg2 ...\n");
  printf("\n");
  printf(" -h/--help\t\tPrint help.\n");
  printf(" -q/--quiet\t\tDo not print the time index when the command is run.\n");
  printf(" -s/--simulate\t\tDo not run the command or sleep. Useful for testing the\n");
  printf("\t\t\trandom process.\n");
  printf("\n");
  printf("The program needs /dev/urandom to work.\n");
  printf("\n");
  printf("Example 1: Play a wav file statistically every 64 seconds. Danger: irritating.\n");
  printf("\tpoissonrun 64 aplay foo.wav\n");
}


int main(int argc, char **argv)
{
  long long i;
  int simulation = 0;
  int quiet = 0;
  char **command;
  int max_rounds = 0;
  int one_shot = 0;
  struct timeval stime = {.tv_sec = 1};
  double prob;

  for (i = 1; i < argc;) {
    if (strcmp(argv[i], "-1") == 0 || strcmp(argv[i], "--one-shot") == 0) {
      one_shot = 1;
      i++;
      continue;
    } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
      print_help();
      return 0;
    } else if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "--max-rounds") == 0) {
      if ((i + 1) >= argc) {
	fprintf(stderr, "Not enough args.\n");
	return -1;
      }
      max_rounds = atoi(argv[i + 1]);
      if (max_rounds <= 0) {
	fprintf(stderr, "Max rounds must be positive.\n");
	return -1;
      }
      i += 2;
      continue;
    } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
      quiet = 1;
      i++;
      continue;
    } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--simulate") == 0) {
      simulation = 1;
      i++;
      continue;
    } else if (strcmp(argv[i], "--sleep") == 0) {
      double sleep_time;
      if ((i + 1) >= argc) {
	fprintf(stderr, "Not enough args.\n");
	return -1;
      }
      sleep_time = atof(argv[i + 1]);
      if (sleep_time <= 0.0) {
	fprintf(stderr, "Time interval must be positive.\n");
	return -1;
      }
      stime.tv_sec = floor(sleep_time);
      stime.tv_usec = 1000000.0 * (sleep_time - floor(sleep_time));
      assert(stime.tv_usec < 1000000);
      i += 2;
      continue;
    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown arg: %s\n", argv[i]);
      return -1;
    }
    break;
  }

  if ((i + 1) >= argc) {
    fprintf(stderr, "Not enough args.\n");
    return -1;
  }


  if (strncmp(argv[i], "p=", 2) == 0) {
    /* Given as a probability rather than time interval */
    prob = atof(argv[i] + 2);
    if (prob <= 0.0) {
      fprintf(stderr, "Probability must be positive.\n");
      return -1;
    }
  } else {
    double interval = atof(argv[i]);
    if (interval <= 0.0) {
      fprintf(stderr, "interval must be positive.\n");
    }
    prob = 1.0 / interval;
  }

  command = argv + i + 1;

  for (i = 0;; i++) {

    if (max_rounds > 0 && i >= max_rounds)
      break;

    if (get_random() < prob) {
      int rv;
      if (quiet == 0)
	fprintf(stderr, "Command run on time index %lld seconds.\n", i);
      if (simulation == 0) {
	rv = fork();
	if (rv == 0) {
	  execvp(command[0], command);
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
      if (one_shot)
	break;
    }
    if (simulation == 0) {
      struct timeval stemp = stime;
      select(0, NULL, NULL, NULL, &stemp);
    }
  }
  return 0;
}
