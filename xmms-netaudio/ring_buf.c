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

#include "ring_buf.h"

/* buf may be zero, in that case init will allocate the buffer. if ring buffer
   allocates the buffer by itself, it will also free() it in
   ring_buffer_destroy(). However, if the 'buf' was given by the user for the
   init, then ring_buffer_destroy() will not free() it.
*/
int ring_buf_init(struct ring_buf_t *r, void *buf, int size)
{
  if (!r) {
    fprintf(stderr, "ring_buf_init: null pointer\n");
    return 0;
  }
  if (size <= 0 || size >= 0x01000000) {
    fprintf(stderr, "ring_buf_init: illegal size (0x%x)\n", size);
    return 0;
  }

  memset(r, 0, sizeof(struct ring_buf_t));

  r->size = size;

  if (buf) {
    /* user gave the buf. this will not be freed in ring_buf_destroy() */
    r->buf = (char *) buf;
    r->given_buf = 1;
  } else {
    /* buffer is allocated. this will be freed in ring_buf_destroy() */  
    r->buf = malloc(r->size);
    r->given_buf = 0;
    if (!r->buf) { 
      fprintf(stderr, "ring_buf_init: malloc failed\n");
      return 0;
    }
  }
  return 1;
}

void ring_buf_destroy(struct ring_buf_t *r)
{
  if (!r) {
    fprintf(stderr, "ring_buf_destroy: tried to free null pointer\n");
    return;
  }
  if (!r->given_buf) {
    if (r->buf) {
      free(r->buf);
    } else {
      fprintf(stderr, "ring_buf_destroy: buffer was zero\n");
    }
  }
  r->buf = 0;
}

void ring_buf_reset(struct ring_buf_t *r)
{
  if (!r) {
    fprintf(stderr, "ring_buf_reset: null pointer\n");
    return;
  }
  r->input_offs = r->output_offs = 0;
}


/* - x ------------ (x is i and o) */
/* --- o SSS i ---- */
/* SSS i --- o SSSS */
int ring_buf_free(struct ring_buf_t *r)
{
  int i, o, ret;
  if (!r) {
    fprintf(stderr, "ring_buf_free: null pointer\n");
    return 0;
  }
  i = r->input_offs;
  o = r->output_offs;
  ret = (o > i) ? (o - i) : (o + r->size - i);
  ret--;
  return ret;
}

int ring_buf_content(struct ring_buf_t *r)
{
  int i, o;
  if (!r) {
    fprintf(stderr, "ring_buf_content: null pointer\n");
    return 0;
  }
  i = r->input_offs;
  o = r->output_offs;
  return (i >= o) ? (i - o) : (i + r->size - o);
}

void ring_buf_put(char *ptr, int len, struct ring_buf_t *r)
{
  int i;
  if (!r) {
    fprintf(stderr, "ring_buf_put: null pointer\n");
    return;
  }
  if (len <= 0) {
    fprintf(stderr, "ring_buf_put: len <= 0\n");
    return;
  }
  if (ring_buf_free(r) < len) {
    fprintf(stderr, "ring_buf_put: overflow\n");
    return;
  }

  i = r->input_offs;
  if ((i + len) <= r->size) {
    memcpy(&r->buf[i], ptr, len);
  } else {
    int f = r->size - i;
    memcpy(&r->buf[i], ptr, f);
    memcpy(r->buf, ptr + f, len - f);
  }
  r->input_offs = (i + len) % r->size;
}

void ring_buf_get(char *dst, int len, struct ring_buf_t *r)
{
  int o;
  if (!r) {
    fprintf(stderr, "ring_buf_get: null pointer\n");
    return;
  }
  if (len <= 0) {
    fprintf(stderr, "xmms-netaudio: ring_buf_get: len <= 0\n");
    return;
  }
  if (ring_buf_content(r) < len) {
    fprintf(stderr, "xmms-netaudio: ring_buf_get: underflow\n");
    return;
  }

  o = r->output_offs;
  if ((o + len) <= r->size) {
    memcpy(dst, &r->buf[o], len);
  } else {
    int f = r->size - o;
    memcpy(dst, &r->buf[o], f);
    memcpy(dst + f, r->buf, len - f);
  }
  r->output_offs = (o + len) % r->size;
}

int ring_buf_process(int (*process)(char *buf, int size, void *arg),
		     void *arg, int max, struct ring_buf_t *r)
{
  int o;
  int len;
  int content;
  int processed;
  if (!r) {
    fprintf(stderr, "ring_buf_get: null pointer\n");
    return 0;
  }

  content = ring_buf_content(r);
  if (content == 0) {
    return 0;
  }
  if (content > max) {
    content = max;
  }

  o = r->output_offs;
  if ((o + content) <= r->size) {
    len = content;
  } else {
    len = r->size - o;
  }
  processed = process(&r->buf[o], len, arg);
  r->output_offs = (o + processed) % r->size;
  return processed;
}
