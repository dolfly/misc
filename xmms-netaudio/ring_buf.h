#ifndef _XMMS_NETAUDIO_FIFO_H_
#define _XMMS_NETAUDIO_FIFO_H_

struct ring_buf_t {
  int size;        /* ring buf size */
  char *buf;       /* ring buffer */
  int given_buf;   /* if zero, ring_buf_init() allocated the 'buf', otherwise
		      ring_buf_init() was given the 'buf'. if this is non-zero
		      ring_buf_destroy() will not free() the 'buf' */
  int input_offs;  /* position where to put stuff */
  int output_offs; /* position from where to get stuff */
};

int ring_buf_init(struct ring_buf_t *r, void *buf, int size);
void ring_buf_destroy(struct ring_buf_t *r);
void ring_buf_reset(struct ring_buf_t *r);

int ring_buf_free(struct ring_buf_t *r);
int ring_buf_content(struct ring_buf_t *r);
void ring_buf_get(char *dst, int len, struct ring_buf_t *r);
void ring_buf_put(char *ptr, int len, struct ring_buf_t *r);

int ring_buf_process(int (*process)(char *buf, int size, void *arg),
		     void *arg, int max, struct ring_buf_t *r);

#endif
