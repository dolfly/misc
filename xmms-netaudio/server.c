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
#include "event.h"

extern int errno;

int dspfd;

struct event_queue eq;

#define MAX_INPUT_SIZE 4096
static const int rbsize = 524288;

struct stream {
  int fd;
  int meta_size;
  long long output;
  int finished;
  struct na_meta meta;
  struct ring_buf_t rb;
};

static struct stream in_stream;

static int init_dsp(int fd, struct na_meta *meta) {
  int is_stereo;
  int fmt, rate, nch;
  int tmp;
  fmt = ntohl(meta->fmt);
  rate = ntohl(meta->rate);
  nch = ntohl(meta->nch);
  fprintf(stderr, "fmt = %x rate = %x nch = %x\n", fmt, rate, nch);
  switch (fmt) {
  case NA_FMT_S16_BE: case NA_FMT_S16_LE: case NA_FMT_S16_NE:
    break;
  default:
    fprintf(stderr, "xmms-netaudio: illegal format (%d)\n", fmt);
    return 0;
  }
  if (rate != 44100) {
    fprintf(stderr, "xmms-netaudio: illegal rate\n");
    return 0;
  }
  if (nch != 2) {
    fprintf(stderr, "xmms-netaudio: illegal rate\n");
    return 0;
  }
  if (ioctl(fd, SNDCTL_DSP_SETFMT, AFMT_S16_LE)) {
    fprintf(stderr, "xmms-netaudio: setfmt failed\n");
    return 0;
  }
  is_stereo = (nch == 2);
  if (ioctl(fd, SNDCTL_DSP_STEREO, &is_stereo)) {
    fprintf(stderr, "xmms-netaudio: stereo failed\n");
  }
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

static void close_dsp(void) {
  /* do dsp reset here */
  while (close(dspfd)) {
    perror("xmms-netaudio: not able to close dsp");
    sleep(1);
  }
  dspfd = -1;
}

static void open_dsp(void *meta) {
  dspfd = open("/dev/dsp", O_WRONLY);
  if (dspfd < 0) {
    perror("xmms-netaudio: can not open audio device");
    return;
  }
  if (!init_dsp(dspfd, meta)) {
    /* do some stuff to stop processing input stream */
    close_dsp();
  }
}

static int stream_input(struct stream *s) {
  int ret;
  int meta_len = (int) sizeof(struct na_meta);
  char buf[MAX_INPUT_SIZE];
  if (s->meta_size < meta_len) {
    char *metabuf = (char *) (&s->meta);
    ret = read(s->fd, metabuf + s->meta_size, meta_len - s->meta_size);
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
    if (s->meta_size == meta_len) {
      event_append(&eq, open_dsp, s);
    }

  } else {
    ret = read(s->fd, buf, sizeof(buf));
    if (ret > 0) {
      ring_buf_put(buf, ret, &s->rb);
    } else if (ret == 0) {
      
    } else {

    }
    fprintf(stderr, "stream_input: ret = %d\n", ret);
  }
  return 1;
}

static int dsp_write(char *buf, int size, void *arg) {
  int fd = (int) arg;
  int ret;
  ret = write(fd, buf, size);
  if (ret < 0) {
    if (errno != EINTR) {
      perror("dsp_write");
    }
    return 0;
  } else if (ret == 0) {
    fprintf(stderr, "xmms-netaudio: interesting: dsp_write returned zero\n");
    return 0;
  }
  return ret;
}

static int dsp_output(int fd, struct stream *s) {
  int ret;
  int content;
  content = ring_buf_content(&s->rb);
  if (content > 0) {
    /* process at most 4096 bytes from ring buffer with dsp_write() */
    ret = ring_buf_process(dsp_write, (void *) fd, 4096, &s->rb);
    fprintf(stderr, "dsp_output %d\n", ret);
  }
  return 1;
}

int main(int argc, char **argv) {
  int i;
  char *port = 0;
  struct pollfd pfd[3];
  int is_connected;
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

  memset(&in_stream, 0, sizeof(struct stream));

  if (!ring_buf_init(&in_stream.rb, 0, rbsize)) {
    fprintf(stderr, "xmms-netaudio: ring buf init failed\n");
    exit(-1);
  }

  if (!event_init(&eq, 64)) {
    fprintf(stderr, "xmms-netaudio: event queue init failed\n");
    exit(-1);
  }

  pfd[0].fd = net_listen(0, port, "tcp");
  pfd[0].events = POLLIN;

  is_connected = 0;
  in_stream.fd = -1;
  in_stream.meta_size = 0;
  dspfd = -1;

  while (1) {

    event_handler(&eq);

    nfds = 1;

    if (in_stream.fd < 0)
      goto no_in_fd;
    if (in_stream.finished)
      goto no_in_fd;
    if (ring_buf_free(&in_stream.rb) < MAX_INPUT_SIZE)
      goto no_in_fd;

    pfd[nfds].fd = in_stream.fd;
    pfd[nfds].events = POLLIN;
    nfds++;
  no_in_fd:

    if (dspfd >= 0) {
      if (ring_buf_content(&in_stream.rb) > 0) {
	pfd[nfds].fd = dspfd;
	pfd[nfds].events = POLLOUT;
	nfds++;
      }
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
	(void) dsp_output(dspfd, &in_stream);
      }
    }
  }
  return 0;
}
