/* reverselines by Heikki Orsila <heikki.orsila@iki.fi>. The source code is
   in public domain. You may do anything with the source code.

   history:
   2005.01.13: version 1
    - initial version
    - supports randomizing with -r or --randomize options
    - force use of /dev/urandom with -c or --check (will give error if
      the device is not available)
    - -0 option uses \0 as the separator instead of \n. Perhaps useful with
      find -print0.
 */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

static const int RL_RAND_MAX = 0x3FFFFFFF;

static FILE *rl_rf = NULL;
static long int rl_seed;
static int rl_must_have_urandom;

static void rl_init_rand(void)
{
  rl_seed = time(0);
  if (rl_seed == -1) {
    fprintf(stderr, "warning. non-random sequence.\n");
    rl_seed = 1;
  }
  if ((rl_rf = fopen("/dev/urandom", "r"))) {
    fread(&rl_seed, 1, sizeof(rl_seed), rl_rf);
  }
  srand48(rl_seed);
}


static int rl_rand(void)
{
  int ret;
  long int rnd;
  if (rl_rf == 0)
    return ((double) (RL_RAND_MAX + 1)) * lrand48() / (2147483648.0 + 1.0);
  ret = fread(&rnd, 1, sizeof(rnd), rl_rf);
  if (ret == 0) {
    if (ferror(rl_rf)) {
      perror("read error in rl_rand");
      exit(-1);
    }
    fprintf(stderr, "strange. end of file with /dev/urandom.\n");
    exit(-1);
  }
  if (ret != sizeof(rnd)) {
    fprintf(stderr, "strange. fread() should return sizeof(rnd) bytes. returned %d\n", ret);
    exit(-1);
  }
  ret = ((double) (RL_RAND_MAX + 1)) * (rnd & 0x7FFFFFFF) / 2147483648.0;
  return ret;
}


void print_help(void)
{
  printf("reverselines %s\n\n", RLVERSION);
  printf("USAGE: reverselines [-0] [-c] [-h] [-r]\n\n");
  printf("DESCRIPTION:\n");
  printf("reverselines reads all lines from stdin, and then prints them in a specific\n");
  printf("order. By default the order is reverse order.\n\n");
  printf(" -0 / --null       Make \\0 as the separator instead of \\n. Potentially\n");
  printf("                   useful with \'find -print0\'.\n");
  printf(" -h / --help       Print help.\n");
  printf(" -c / --check      Force /dev/urandom check for -r.\n");
  printf(" -r / --randomize  Print out in random order.\n\n");
  
  printf("PROBLEMS:\n");
  printf("Operating systems which don't have /dev/urandom use time(0) to initialize\n");
  printf("seed for srand48(). With -c option, error is given if /dev/urandom is not\n");
  printf("available.\n\n");
  printf("AUTHOR: Heikki Orsila <heikki.orsila@iki.fi>\n");
  printf("COPYING: The program, including the source, is public domain.\n");
}


int main(int argc, char **argv)
{
  size_t maxsize;
  size_t used;
  char *buf = NULL;
  char *newbuf;
  size_t ret;
  size_t ind;
  size_t lines;
  size_t lineind;
  char **lineptrs = NULL;
  int beginning;
  int randomize = 0;
  char separator = '\n';

  ind = 1;
  while (ind < ((size_t) argc)) {
    if (strcmp(argv[ind], "-0") == 0 || strcmp(argv[ind], "--null") == 0) {
      separator = '\0';
      ind++;
      continue;
    }

    if (strcmp(argv[ind], "-c") == 0 || strcmp(argv[ind], "--check") == 0) {
      rl_must_have_urandom = 1;
      ind++;
      continue;
    }

    if (strcmp(argv[ind], "-h") == 0 || strcmp(argv[ind], "--help") == 0) {
      print_help();
      exit(0);
    }

    if (strcmp(argv[ind], "-r") == 0 || strcmp(argv[ind], "--randomize") == 0) {
      randomize = 1;
      rl_init_rand();
      ind++;
      continue;
    }

    fprintf(stderr, "%s: unknown arg %s\n", argv[0], argv[ind]);
    print_help();
    goto error;
  }

  if (randomize && rl_must_have_urandom && rl_rf == NULL) {
    fprintf(stderr, "could not initialize urandom\n");
    goto error;
  }

  maxsize = 4096;

  if (!(buf = malloc(maxsize))) {
    perror("no memory");
    goto error;
  }

  used = 0;
  lines = 0;

  while (used < maxsize) {
    ret = fread(buf + used, 1, maxsize - used, stdin);
    if (ret == 0)
      break;

    ind = 0;
    while (ind < ret) {
      if (buf[used + ind] == separator)
	lines++;
      ind++;
    }

    used += ret;

    if (used == maxsize) {
      maxsize *= 2;
      newbuf = realloc(buf, maxsize);
      if (!newbuf) {
	perror("no realloc memory");
	goto error;
      }
      buf = newbuf;
    }
  }

  if (used > 0) {
    if (buf[used - 1] != separator)
      lines++;
  }

  if (!(lineptrs = malloc(sizeof(char *) * lines))) {
    perror("no memory for lineptrs");
    goto error;
  }

  ind = 0;
  lineind = 0;
  beginning = 1;

  while (ind < used) {

    if (beginning) {
      if (lineind >= lines) {
	fprintf(stderr, "fatal error. lineind >= lines. please report.\n");
	goto error;
      }
      lineptrs[lineind++] = buf + ind;
      beginning = 0;
    }

    while (ind < used && buf[ind] != separator)
      ind++;

    if (buf[ind] == separator) {
      beginning = 1;
      ind++;
    }
  }

  lineind = lines - 1;

  while (1) {
    size_t linelen;
    size_t lineoffs;
    char *line;

    if (randomize) {
      size_t randval;
      char *tmp;
      randval = (((double) (lineind + 1)) * rl_rand() / (RL_RAND_MAX + 1.0));
      tmp = lineptrs[randval];
      lineptrs[randval] = lineptrs[lineind];
      lineptrs[lineind] = tmp;
    }

    line = lineptrs[lineind];
    lineoffs = ((intptr_t) line) - ((intptr_t) buf);
    linelen = 0;

    while ((lineoffs + linelen) < used && line[linelen] != separator)
      linelen++;

    while (linelen > 0) {
      ret = fwrite(line, 1, linelen, stdout);
      if (ret == 0) {
	if (ferror(stdout)) {
	  perror("write error");
	  goto error;
	}
	perror("interesting condition fwrite() == 0. report this.");
	goto error;
      }
      line += ret;
      linelen -= ret;
    }

    while ((ret = fwrite(&separator, 1, 1, stdout)) != 1) {
      if (ferror(stdout)) {
	perror("write error");
	goto error;
      }
      fprintf(stderr, "interesting condition while writing separator (%ld)\n", (long) ret);
      goto error;
    }

    if (lineind == 0)
      break;
    lineind--;
  }

  free(buf);
  free(lineptrs);
  return 0;

  error:
  free(buf);
  free(lineptrs);
  return -1;
}
