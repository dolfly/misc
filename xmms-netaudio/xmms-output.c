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

#include <glib.h>
#include <gtk/gtk.h>

#include <xmms/plugin.h>

#include "net.h"

extern int errno;

static int na_valid;

static long long na_input_bytes, na_output_bytes;
static int na_sockfd;

static AFormat na_format;
static int na_rate;
static int na_channels;
static int na_cps;

static const int na_queue_size = 524288;
static char *na_queue;
static int na_input_offs; /* position where to put stuff */
static int na_output_offs; /* position from where to get stuff */

/* - x ------------ (x is i and o) */
/* --- o SSS i ---- */
/* SSS i --- o SSSS */
static int na_queue_free(void) {
  int i = na_input_offs, o = na_output_offs;
  int ret = (o > i) ? (o - i) : (o + na_queue_size - i);
  ret--;
  return ret;
}

static int na_queue_content(void) {
  int i = na_input_offs, o = na_output_offs;
  return (i >= o) ? (i - o) : (i + na_queue_size - o);
}


static void na_queue_put(char *ptr, int len) {
  int i = na_input_offs;

  if (len <= 0) {
    fprintf(stderr, "xmms-netaudio: na_queue_put: len <= 0\n");
    return;
  }
  if (na_queue_free() < len) {
    fprintf(stderr, "xmms-netaudio: na_queue_put: overflow\n");
    return;
  }

  if ((i + len) <= na_queue_size) {
    memcpy(&na_queue[i], ptr, len);
  } else {
    int f = na_queue_size - i;
    memcpy(&na_queue[i], ptr, f);
    memcpy(&na_queue[i + f], ptr + f, len - f);
  }
  i = (i + len) % na_queue_size;
  na_input_offs = i;
}

static void na_queue_get(char *dst, int len) {
  int o = na_output_offs;

  if (len <= 0) {
    fprintf(stderr, "xmms-netaudio: na_queue_get: len <= 0\n");
    return;
  }
  if (na_queue_content() < len) {
    fprintf(stderr, "xmms-netaudio: na_queue_get: underflow\n");
    return;
  }

  if ((o + len) <= na_queue_size) {
    memcpy(dst, &na_queue[o], len);
  } else {
    int f = na_queue_size - o;
    memcpy(dst, &na_queue[o], f);
    memcpy(dst + f, &na_queue[o + f], len - f);
  }
  o = (o + len) % na_queue_size;
  na_output_offs = o;
}

static void na_init(void) {
  fprintf(stderr, "na_init\n");
  na_queue = malloc(na_queue_size);
  if (!na_queue) {
    fprintf(stderr, "xmms-netaudio: na_init: malloc failed\n");
    na_valid = 0;
    return;
  }
  na_valid = 1;
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

static int na_open_audio(AFormat fmt, int rate, int nch) {
  int ret;
  fprintf(stderr, "na_open_audio\n");
  if (!na_valid) {
    fprintf(stderr, "xmms-netaudio: init failed, but open was called\n");
    return 0;
  }
  na_sockfd = net_open("shd.ton.tut.fi", "5555", "tcp");
  if (na_sockfd < 0) {
    /* return 0; */
  }
  na_rate = rate;
  na_channels = nch;
  na_format = fmt;
  ret = typesize(fmt);
  na_cps = ret * rate * nch;

  na_input_offs = na_output_offs = 0;
  na_input_bytes = na_output_bytes = 0;
  return 1;
}

static void na_write_audio(void *ptr, int length) {
  na_input_bytes += length;
  if (length <= 0) {
    fprintf(stderr, "xmms-netaudio: na_write_audio: length <= 0\n");
    return;
  }
  if (na_queue_free() < length) {
    fprintf(stderr, "xmms-netaudio: na_write_audio: not enough space\n");
    return;
  }
  na_queue_put((char *) ptr, length);
  na_output_offs = na_input_offs;
  na_output_bytes += length;
}

static void na_send(void *ptr, int length) {
  char *buf;
  int ret, written;
  struct pollfd pfd;

  buf = (char *) ptr;
  written = 0;
  while (written < length) {

    ret = poll(&pfd, 1, 1000);
    if (ret < 0) {
      if (errno != EINTR) {
	perror("xmms-netaudio: poll returned error");
	break;
      }
    } else if (ret == 0) {
      continue;
    }

    ret = write(na_sockfd, &buf[written], length - written);
    if (ret > 0) {
      na_output_bytes += ret;

    } else if (ret == 0) {
      fprintf(stderr, "xmms-netaudio: na_write_audio: write returned 0\n");
      break;

    } else {
      if (errno != EINTR) {
	perror("xmms-netaudio: na_write_audio");
	break;
      }
    }
  }
}

static void na_close_audio(void) {
  if (na_sockfd >= 0) {
    shutdown(na_sockfd, SHUT_RDWR);
    while (close(na_sockfd));
  }
  na_sockfd = -1;
}

static void na_flush(int time) {
}

static void na_pause(short paused) {
  paused = paused;
}

static int na_buffer_free(void) {
  return na_queue_free();
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
