/* Copyright (c) 2003 Heikki Orsila <heikki.orsila@tut.fi>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/poll.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <pthread.h>

#include <glib.h>
#include <gtk/gtk.h>

#include <xmms/plugin.h>
#include <xmms/util.h>

#include "net.h"
#include "meta.h"
#include "ring_buf.h"

extern int errno;

static int na_valid;
static int na_playing;

static pthread_t na_pth;

static long long na_input_bytes, na_output_bytes;
static int na_sockfd;

static AFormat na_format;
static int na_rate;
static int na_channels;
static int na_cps;

struct ring_buf_t rb;

static void na_init(void) {
  const int na_queue_size = 524288;
  na_valid = 0;
  if (!ring_buf_init(&rb, 0, na_queue_size)) {
    fprintf(stderr, "xmms-netaudio: na_init: no ring buffer\n");
    return;
  }
  na_valid = 1;
  na_playing = 0;
}

static int typesize(AFormat fmt) {
  int ret;
  switch (fmt) {
  case FMT_U8: case FMT_S8:
    ret = 1;
    break;
  case FMT_U16_LE: case FMT_U16_BE: case FMT_U16_NE: case FMT_S16_LE:
  case FMT_S16_BE: case FMT_S16_NE:
    ret = 2;
    break;
  default:
    fprintf(stderr, "xmms-netaudio: typesize: unknown format\n");
    ret = 2;
    break;
  };
  return ret;
}

static void na_close_socket(int fd) {
  if (fd >= 0) {
    shutdown(fd, SHUT_RDWR);
    while (close(fd));
  }
}


static int na_send(int sockfd, void *ptr, int length) {
  char *buf;
  int ret, written;
  struct pollfd pfd;

  if (sockfd < 0)
    return 0;

  pfd.fd = sockfd;
  pfd.events = POLLOUT;

  buf = (char *) ptr;
  written = 0;
  while (written < length) {

    ret = poll(&pfd, 1, 1000);
    fprintf(stderr, "poll ret = %d\n", ret);
    if (ret < 0) {
      if (errno != EINTR) {
	perror("xmms-netaudio: poll returned error");
	break;
      }
    } else if (ret == 0) {
      continue;
    }

    ret = write(sockfd, &buf[written], length - written);
    fprintf(stderr, "send write = %d\n", ret);
    if (ret > 0) {
      written += ret;

    } else if (ret == 0) {
      fprintf(stderr, "xmms-netaudio: na_send: write returned 0\n");
      break;

    } else {
      if (errno != EINTR) {
	perror("xmms-netaudio: na_send");
	return 0;
      }
    }
  }
  return 1;
}

static void *na_write_loop(void *arg) {
  const int s = 512;
  char buf[s];
  int ret;
  arg = arg;
  while (na_playing) {
    ret = ring_buf_content(&rb);
    if (ret > s) {
      ring_buf_get(buf, s, &rb);
      if (na_sockfd >= 0) {
	if (!na_send(na_sockfd, buf, s)) {
	  na_close_socket(na_sockfd);
	  na_sockfd = -1;
	}
      }
      na_output_bytes += s;
    } else {
      xmms_usleep(10000);
    }
  }
  return 0;
}

static int na_send_meta(AFormat fmt, int rate, int nch) {
  struct na_meta m;
  m.fmt = htonl(fmt);
  m.rate = htonl(rate);
  m.nch = htonl(nch);
  return na_send(na_sockfd, (char *) &m, sizeof(m));
}

static int na_open_audio(AFormat fmt, int rate, int nch) {
  int ret, tries;
  if (!na_valid) {
    fprintf(stderr, "xmms-netaudio: init failed, but open was called\n");
    return 0;
  }

  tries = 0;
  na_sockfd = -1;
  while (tries < 20) {
    na_sockfd = net_open("shd.ton.tut.fi", "5555", "tcp");
    if (na_sockfd >= 0)
      break;
    tries++;
    xmms_usleep(500000);
  }
  if (na_sockfd < 0) {
    fprintf(stderr, "xmms-netaudio: timeout: couldn't connect to remote server\n");
    return 0;
  }

  if (!na_send_meta(fmt, rate, nch)) {
    fprintf(stderr, "xmms-netaudio: couldn't send meta data to remote server\n");
    na_close_socket(na_sockfd);
    return 0;
  }

  na_rate = rate;
  na_channels = nch;
  na_format = fmt;
  ret = typesize(fmt);
  na_cps = ret * rate * nch;

  ring_buf_reset(&rb);
  na_input_bytes = na_output_bytes = 0;

  na_playing = 1;

  pthread_create(&na_pth, 0, na_write_loop, 0);

  return 1;
}

static void na_write_audio(void *ptr, int length) {
  na_input_bytes += length;
  if (length <= 0) {
    fprintf(stderr, "xmms-netaudio: na_write_audio: length <= 0\n");
    return;
  }
  if (ring_buf_free(&rb) < length) {
    fprintf(stderr, "xmms-netaudio: na_write_audio: not enough space\n");
    return;
  }
  ring_buf_put((char *) ptr, length, &rb);
  fprintf(stderr, "%d\n", length);
}

static void na_close_audio(void) {
  na_playing = 0;

  if (pthread_join(na_pth, 0)) {
    fprintf(stderr, "xmms-netaudio na_close_audio: thread_join failed\n");
  }

  na_close_socket(na_sockfd);
  na_sockfd = -1;
}

static void na_flush(int time) {
  time = time;
  /* what should we do here? */
}

static void na_pause(short paused) {
  paused = paused;
}

static int na_buffer_free(void) {
  return ring_buf_free(&rb);
}

static int na_buffer_playing(void) {
  /* disk writer plugin always returns zero, why? */
  return 0;
}

static int na_output_time(void) {
  if (na_cps == 0)
    return 0;
  /* fprintf(stderr, "output: na_output_bytes = %lld na_cps = %d\n", na_output_bytes, na_cps); */
  return (int) (na_output_bytes * 1000 / na_cps);
}

static int na_written_time(void) {
  if (na_cps == 0)
    return 0;
  /* fprintf(stderr, "written: na_input_bytes = %lld na_cps = %d\n", na_input_bytes, na_cps); */
  return (int) (na_input_bytes * 1000 / na_cps);
}

static void na_configure(void) {
}

static OutputPlugin op = {
  0, /* handle */
  0, /* filename */
  0, /* description */
  na_init, /* init */
  0, /* about */
  na_configure, /* configure */
  0, /* get_volume */
  0, /* set_volume */
  na_open_audio, /* open audio */
  na_write_audio, /* write audio */
  na_close_audio, /* close audio */
  na_flush, /* flush */
  na_pause, /* pause */
  na_buffer_free, /* buffer free */
  na_buffer_playing, /* buffer playing */
  na_output_time, /* output time */
  na_written_time /* written time */
};

OutputPlugin *get_oplugin_info(void) {
  op.description = strdup("xmms-netaudio output plugin");
  if (!op.description) {
    return 0;
  }
  return &op;
}
