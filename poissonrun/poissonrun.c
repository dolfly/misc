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
  printf("\n"
         "The program takes a PROBSPEC and a command as inputs. The command is run\n"
	 "at random times depending on the PROBSPEC. PROBSEC will determine value T so\n"
	 "that the command will be run every T seconds on average. Each run is\n"
	 "independent of other runs. If the command is run at time t, it is not less\n"
	 "likely to be run at time t+1 (immediately following second).\n"
	 "\n"
	 "The probability specification PROBSPEC can be given in two forms. In the first\n"
	 "case, PROBSPEC is a pure value (T), so that it is a time interval in seconds.\n"
	 "The command will be run every T second on average. In the second case, PROBPEC\n"
	 "has form p=x, where x is a probability of the command being run at any random\n"
	 "invocation. For example, if x=0.1, the event occurs once in 10 runs on average.\n"
	 "\n"
	 "Randomization happens once every second, or if \"--sleep t\" it happens every\n"
	 "\"t\" seconds. However, only one instance of the command is being run at any\n"
	 "given time, unless -f option is used (fork and forget), in which case many\n"
	 "commands can happen simultaneously.\n"
	 "\n"
	 "Time interval T should be big compared to command execution time so that this\n"
	 "process is statistically reasonable.\n"
	 "Time interval T should be given as a floating-point number bigger than 1.\n"
	 "\n"
	 " USAGE:\n"
	 "\tpoissonrun [-e n] [-m n] [-q] [-s] [--sleep t] PROBSPEC command args ...\n"
	 "\n"
	 " -e n/--max-events n  Stop when command has been executed n times\n"
	 " -f                   Fork and forget; multiple commands can be running\n"
	 "                      simultaneously\n"
	 " -h/--help            Print help\n"
	 " -m n/--max-rounds n  Stop after n randomizations (compare to -e)\n"
	 " -q/--quiet           Do not print the time index when the command is run\n"
	 " -s/--simulate        Do not run the command or sleep. Useful for testing the\n"
	 "                      random process.\n"
	 " --sleep t            Sleep t seconds between random invocations (useful for\n"
	 "                      time intervals < 2 seconds)\n"
	 " -v/--version         Print poissonrun version number\n"
	 "\n"
	 "The program needs /dev/urandom to work.\n"
	 "\n"
	 "Example 1: Play a wav file statistically every 64 seconds. (irritating)\n"
	 "\n"
	 "\tpoissonrun 64 aplay foo.wav\n"
	 "\n"
	 "Example 2: Play a wav file statistically every second. Set sleep time\n"
	 "interval to 0.1s and probability to 0.1. Allow multiple samples to be played\n"
	 "simultaneously. (more irritating than the previous example :-)\n"
	 "\n"
	 "\tpoissonrun -f --sleep 0.1 p=0.1 aplay foo.wav\n"
	 "\n");
}


int main(int argc, char **argv)
{
  long long i;
  int simulation = 0;
  int quiet = 0;
  char **command;
  size_t max_rounds = 0;
  size_t max_events = 0;
  long value;
  double sleep_time = 1.0;
  double prob;
  struct timeval intervaltimeval;
  char *endptr;
  int fork_and_forget = 0;

  for (i = 1; i < argc;) {

    if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--max-events") == 0) {
      if ((i + 1) >= argc) {
	fprintf(stderr, "Not enough args.\n");
	return -1;
      }
      value = strtol(argv[i + 1], &endptr, 10);
      if (*endptr != 0 || value <= 0) {
	fprintf(stderr, "Invalid max events\n");
	return -1;
      }
      max_events = (size_t) value;
      i += 2;
      continue;

    } else if (strcmp(argv[i], "-f") == 0) {
      fork_and_forget = 1;
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
      value = strtol(argv[i + 1], &endptr, 10);
      if (*endptr != 0 || value <= 0) {
	fprintf(stderr, "Max rounds must be positive.\n");
	return -1;
      }
      max_rounds = value;
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
      if ((i + 1) >= argc) {
	fprintf(stderr, "Not enough args.\n");
	return -1;
      }
      sleep_time = atof(argv[i + 1]);
      if (sleep_time <= 0.0) {
	fprintf(stderr, "Time interval must be positive.\n");
	return -1;
      }
      i += 2;
      continue;

    } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
      printf("poissonrun " VERSION "\n");
      exit(0);

    } else if (argv[i][0] == '-') {
      fprintf(stderr, "Unknown arg: %s\n", argv[i]);
      return -1;
    }
    break;
  }

  if ((i + 1) >= argc) {
    fprintf(stderr, "Not enough args. Run %s -h\n", argv[0]);
    return -1;
  }


  if (strncmp(argv[i], "p=", 2) == 0) {
    /* Given as a probability rather than time interval */
    prob = atof(argv[i] + 2);
    if (prob <= 0.0) {
      fprintf(stderr, "Probability must be positive.\n");
      return -1;
    }
    if (prob > 1.0) {
      fprintf(stderr, "Probability must not be over 1.0.\n");
      return -1;
    }
  } else {
    double interval = atof(argv[i]);
    if (interval <= 0.0) {
      fprintf(stderr, "Interval must be positive.\n");
      return -1;
    }
    if (interval < 2.0 * sleep_time) {
      fprintf(stderr, "Warning: interval should be much less than the sleep time (%f). Choose another sleep time or greater interval value.\n", sleep_time);
    }
    if (interval < sleep_time) {
      fprintf(stderr, "Interval must be at least as long as the sleep time for one round.\n");
      return -1;
    }
    prob = sleep_time / interval;
  }

  command = argv + i + 1;

  intervaltimeval.tv_sec = floor(sleep_time);
  intervaltimeval.tv_usec = 1000000.0 * (sleep_time - floor(sleep_time));
  assert(intervaltimeval.tv_usec < 1000000);

  for (i = 0;; i++) {

    if (max_rounds > 0 && i >= max_rounds)
      break;

    if (get_random() < prob) {
      int rv;
      if (quiet == 0)
	fprintf(stderr, "Command run on iteration %lld (%lld * sleep_time = %.3lfs)\n", i, i, i * sleep_time);
      if (simulation == 0) {

	rv = fork();
	if (rv == 0) {

	  /* In fork and forget mode, fork second time so that init will
	     automagically inherit children instead of waiting() for them */
	  if (fork_and_forget) {
	    rv = fork();
	    /* The father (rv < 0 and rv > 0) must abort */
	    if (rv < 0) {
	      perror("Not able to fork");
	      abort();
	    } else if (rv > 0) {
	      abort();
	    }
	  }
	  execvp(command[0], command);
	  abort();

	} else if (rv < 0) {
	  perror("Not able to fork");

	} else {
	  /* No need to wait() in fork and forget mode */
	  while (!fork_and_forget) {
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
      if (max_events == 1)
	break;
      if (max_events > 0)
	max_events--;
    }
    if (simulation == 0) {
      struct timeval tv = intervaltimeval;
      select(0, NULL, NULL, NULL, &tv);
    }
  }
  return 0;
}
