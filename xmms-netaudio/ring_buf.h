#ifndef _XMMS_NETAUDIO_FIFO_H_
#define _XMMS_NETAUDIO_FIFO_H_

struct ring_buf_t {
  int size;        /* ring buf size */
  char *queue;     /* ring buffer */
  int input_offs;  /* position where to put stuff */
  int output_offs; /* position from where to get stuff */
};

void ring_buf_get(char *dst, int len, struct ring_buf_t *q);
void ring_buf_put(char *ptr, int len, struct ring_buf_t *q);

#endif

