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

static struct event_queue eq;

#define MAX_INPUT_SIZE 4096

static const int rbsize = 16384;

struct stream {
  int valid;
  int fd;
  int meta_size;
  long long output;
  int finished;
  struct na_meta meta;
  struct ring_buf_t rb;
};

static struct stream in_stream;
static struct stream dsp_stream;


static int init_dsp(int fd, struct na_meta *meta) {
  int is_stereo;
  int fmt, rate, nch;
  int dspfmt, tmp;
  unsigned long formats;
  fmt = meta->fmt;
  rate = meta->rate;
  nch = meta->nch;
  switch (fmt) {
  case NA_FMT_S16_LE: dspfmt = AFMT_S16_LE; break;
  case NA_FMT_S16_NE: dspfmt = AFMT_S16_NE; break;
  default:
    fprintf(stderr, "xmms-netaudio: illegal format (%d)\n", fmt);
    return 0;
  }
  if (rate != 44100) {
    fprintf(stderr, "xmms-netaudio: illegal rate (%d)\n", rate);
    return 0;
  }
  if (nch != 2) {
    fprintf(stderr, "xmms-netaudio: illegal number of channels (%d)\n", nch);
    return 0;
  }

  if (ioctl(fd, SNDCTL_DSP_GETFMTS, &formats)) {
    perror ("xmms-netaudio: getfmts failed");
    return 0;
  }

  tmp = 0x00040000 + 12;
  if (ioctl(fd, SNDCTL_DSP_SETFRAGMENT, &tmp)) {
    perror ("xmms-netaudio: setfragment failed");
  }

  tmp = dspfmt;
  if (ioctl(fd, SNDCTL_DSP_SETFMT, &tmp)) {
    perror("xmms-netaudio: setfmt failed");
    return 0;
  }
  is_stereo = (nch == 2);
  if (ioctl(fd, SNDCTL_DSP_STEREO, &is_stereo)) {
    perror("xmms-netaudio: stereo failed");
    return 0;
  }
  if (ioctl(fd, SNDCTL_DSP_SPEED, &rate)) {
    perror("xmms-netaudio: rate failed");
    return 0;
  }
  ioctl (fd, SOUND_PCM_READ_RATE, &tmp);
  /* Some soundcards have a bit of tolerance here (10%) */
  if (tmp < (rate * 9 / 10) || tmp > (rate * 11 / 10)) {
    fprintf (stderr, "xmms-netaudio: can't use sound with desired frequency (%d)\n", rate);
    return 0;
  }
  return 1;
}

static void close_stream(struct stream *s) {
  if (s->fd < 0)
    return;
  while (close(s->fd)) {
    perror("xmms-netaudio: not able to close stream");
    sleep(1);
  }
  s->fd = -1;
  if (s->rb.buf) {
    ring_buf_destroy(&s->rb);
  }
  s->valid = 0;
  fprintf(stderr, "xmms-netaudio: stream closed\n");
}


static void open_dsp(void *meta) {
  dsp_stream.fd = open("/dev/dsp", O_WRONLY);
  if (dsp_stream.fd < 0) {
    perror("xmms-netaudio: can not open audio device");
    close_stream(&in_stream);
    return;
  }
  if (!init_dsp(dsp_stream.fd, (struct na_meta *) meta)) {
    /* do some stuff to stop processing input stream */
    close_stream(&in_stream);
    close_stream(&dsp_stream);
  }
  dsp_stream.valid = 1;
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
    /* dump_buf((unsigned char *) (&s->meta), meta_len); */
    if (s->meta_size == meta_len) {
      /* fix values to native endian */
      s->meta.fmt = ntohl(s->meta.fmt);
      s->meta.rate = ntohl(s->meta.rate);
      s->meta.nch = ntohl(s->meta.nch);
      /* setup open dsp event to be executed */
      event_append(&eq, open_dsp, &s->meta);
    }

  } else {
    int free = ring_buf_free(&s->rb);
    if (free > 0) {
      free = (free <= ((int) sizeof(buf))) ? free : sizeof(buf);
      ret = read(s->fd, buf, sizeof(buf));
      if (ret > 0) {
	ring_buf_put(buf, ret, &s->rb);
      } else if (ret == 0) {
	fprintf(stderr, "input stream eof\n");
	s->finished = 1;
	return 1;
      } else {
	perror("input stream input error");
	s->finished = 1;
	return 0;
      }
    } else {
      fprintf(stderr, "no space in ring buf. stall.\n");
      sleep(1);
    }
  }
  return 1;
}

static int dsp_write(char *buf, int size, void *arg) {
  int fd = (int) arg;
  int ret;
  ret = write(fd, buf, size);
  if (ret < 0) {
    if (errno != EINTR) {
      perror("xmms-netaudio: dsp_write");
      close_stream(&dsp_stream);
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
    fprintf(stderr, "xmms-netaudio: not enough parameters. give a port to listen. use -p port.\n");
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
  in_stream.valid = 0;

  dsp_stream.fd = -1;
  dsp_stream.valid = 0;

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

    if (!dsp_stream.valid || dsp_stream.fd < 0)
      goto no_dsp_fd;
    if (ring_buf_content(&in_stream.rb) == 0)
      goto no_dsp_fd;

    pfd[nfds].fd = dsp_stream.fd;
    pfd[nfds].events = POLLOUT;
    nfds++;

  no_dsp_fd:

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

      if (in_stream.valid) {
	while (close(in_stream.fd)) {
	  fprintf(stderr, "xmms-netaudio: closing existing input stream\n");
	  sleep(1);
	}
	in_stream.fd = -1;
	in_stream.finished = 0;
	in_stream.meta_size = -1;
	in_stream.valid = 0;
      }

      in_stream.fd = accept(pfd[0].fd, 0, 0);
      if (in_stream.fd < 0) {
	perror("xmms-netaudio: accept error");
	exit(-1);
      }
      in_stream.valid = 1;
    }

    for (i = 1; i < nfds; i++) {
      if (in_stream.valid) {
	if (pfd[i].fd == in_stream.fd) {
	  if (pfd[i].events & POLLIN) {
	    (void) stream_input(&in_stream);
	  }
	}
      }
      if (dsp_stream.valid) {
	if (pfd[i].fd == dsp_stream.fd) {
	  if (pfd[i].revents & POLLOUT) {
	    (void) dsp_output(dsp_stream.fd, &in_stream);
	  }
	}
      }
    }
  }
  return 0;
}
