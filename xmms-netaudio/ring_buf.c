#include <stdlib.h>
#include <stdio.h>

#include "ring_buf.h"

int ring_buf_init(struct ring_buf_t *r, int size) {
  if (!r) {
    fprintf(stderr, "ring_buf_init: null pointer\n");
    return 0;
  }
  if (size <= 0 || size >= 0x40000000) {
    fprintf(stderr, "ring_buf_init: illegal size (0x%x)\n", size);
    return 0;
  }
  r->size = size;
  memset(r, 0, sizeof(struct ring_buf_t));
  r->buf = malloc(r->size);
  if (!r->buf) {
    fprintf(stderr, "ring_buf_init: malloc failed\n");
    return 0;
  }
  return 1;
}

void ring_buf_destroy(struct ring_buf_t *r) {
  if (!r) {
    fprintf(stderr, "ring_buf_destroy: tried to free null pointer\n");
    return;
  }
  free(r->buf);
  r->buf = 0;
}

void rinf_buf_reset(struct ring_buf_t *r) {
  if (!r) {
    fprintf(stderr, "ring_buf_reset: null pointer\n");
    return;
  }
  r->input_offs = r->output_offs = 0;
}


/* - x ------------ (x is i and o) */
/* --- o SSS i ---- */
/* SSS i --- o SSSS */
int ring_buf_free(struct ring_buf_t *r) {
  int i, o, ret;
  if (!r) {
    fprintf(stderr, "ring_buf_free: null pointer\n");
    return 0;
  }
  i= r->input_offs;
  o = r->output_offs;
  ret = (o > i) ? (o - i) : (o + r->size - i);
  ret--;
  return ret;
}

int ring_buf_content(struct ring_buf_t *r) {
  int i, o;
  if (!r) {
    fprintf(stderr, "ring_buf_content: null pointer\n");
    return 0;
  }
  i = r->input_offs;
  o = r->output_offs;
  return (i >= o) ? (i - o) : (i + r->size - o);
}

void ring_buf_put(char *ptr, int len, struct ring_buf_t *r) {
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
    memcpy(&r->buf[i + f], ptr + f, len - f);
  }
  i = (i + len) % r->size;
  r->input_offs = i;
}

void ring_buf_get(char *dst, int len, struct ring_buf_t *r) {
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
    memcpy(dst + f, &r->buf[o + f], len - f);
  }
  o = (o + len) % r->size;
  r->output_offs = o;
}
