#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <errno.h>

#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "net.h"
#include "meta.h"
#include "ring_buf.h"

extern int errno;

static const int rbsize = 524288;

struct stream_t {
  int fd;
  int meta_size;
  long long output;
  struct na_meta meta;
  struct ring_buf_t rb;
};

static struct stream_t in_stream;

static int init_dsp(int fd, struct na_meta *meta) {
  int is_stereo;
  int rate;
  int tmp;
  if (meta->fmt != NA_FMT_S16_LE) {
    fprintf(stderr, "xmms-netaudio: illegal format\n");
    return 0;
  }
  if (meta->rate != 44100) {
    fprintf(stderr, "xmms-netaudio: illegal rate\n");
    return 0;
  }
  if (meta->nch != 2) {
    fprintf(stderr, "xmms-netaudio: illegal rate\n");
    return 0;
  }

  if (ioctl(fd, SNDCTL_DSP_SETFMT, AFMT_S16_LE)) {
    fprintf(stderr, "xmms-netaudio: setfmt failed\n");
    return 0;
  }
  is_stereo = meta->nch;
  if (ioctl(fd, SNDCTL_DSP_STEREO, &is_stereo)) {
    fprintf(stderr, "xmms-netaudio: stereo failed\n");
  }
  rate = (int) meta->rate;
  if (ioctl(fd, SNDCTL_DSP_SPEED, &rate)) {
    fprintf(stderr, "xmms-netaudio: rate failed\n");
  }
  ioctl (fd, SOUND_PCM_READ_RATE, &tmp);
  /* Some soundcards have a bit of tolerance here (10%) */
  if (tmp < (rate * 9 / 10) || tmp > (rate * 11 / 10)) {
    fprintf (stderr, "xmms-netaudio: can't use sound with desired frequency (%d)\n", rate);
    return 0;
  }
  return 1;
}

static int stream_input(struct stream_t *s) {
  int ret;
  int meta_len = (int) sizeof(struct na_meta);
  char buf[65536];

  if (s->meta_size < meta_len) {
    ret = read(s->fd, &buf[s->meta_size], meta_len - s->meta_size);
    if (ret == 0) {
      fprintf(stderr, "xmms-netaudio: couldn't get meta -> kill stream\n");
      return 0;
    } else if (ret < 0) {
      if (errno != EINTR) {
	fprintf(stderr, "xmms-netaudio: stream error when getting meta\n");
	return 0;
      }
    } else {
      s->meta_size += ret;
    }
  } else {
    ret = read(s->fd, buf, sizeof(buf));
    fprintf(stderr, "stream_input: ret = %d\n", ret);
  }
  return 1;
}

static int dsp_output(int fd) {
  fd = fd;
  return 0;
}

int main(int argc, char **argv) {
  int i;
  char *port = 0;
  struct pollfd pfd[3];
  int is_connected;
  int nfds;
  int ret;
  int dspfd;

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

  memset(&in_stream, 0, sizeof(struct stream_t));

  if (!ring_buf_init(&in_stream.rb, 0, rbsize)) {
    fprintf(stderr, "xmms-netaudio: ring buf init failed\n");
    exit(-1);
  }

  pfd[0].fd = net_listen(0, port, "tcp");
  pfd[0].events = POLLIN;

  is_connected = 0;
  in_stream.fd = -1;
  in_stream.meta_size = 0;
  dspfd = -1;

  while(1) {
    nfds = 1;
    if (in_stream.fd >= 0) {
      pfd[nfds].fd = in_stream.fd;
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
      in_stream.fd = accept(pfd[0].fd, 0, 0);
      if (in_stream.fd < 0) {
	perror("xmms-netaudio: accept error");
	exit(-1);
      }
      pfd[1].fd = in_stream.fd;
      pfd[1].events = POLLOUT;
    }

    for (i = 1; i < nfds; i++) {
      if (pfd[i].fd == in_stream.fd) {
	(void) stream_input(&in_stream);
      } else if (pfd[i].fd == dspfd) {
	/* test here if there's something to write */
	(void) dsp_output(dspfd);
      }
    }
  }
  return 0;
}
