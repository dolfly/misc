/* Copyright (c) 2003 Heikki Orsila <heikki.orsila@tut.fi> */

#include <stdlib.h>
#include <stdio.h>

#include "event.h"

int event_append(struct event_queue *q, struct event *e) {
  if (q->n >= q->max) {
    fprintf(stderr, "event queue full\n");
    return 0;
  }
  memcpy(&q->list[q->n], e, sizeof(struct event));
  q->n++;
  return 1;
}

int event_init(struct event_queue *q, int max) {
  q->n = 0;
  q->list = malloc(sizeof(struct event) * max);
  if (!q->list) {
    fprintf(stderr, "no memory for event queue\n");
    return 0;
  }
  return 1;
}
