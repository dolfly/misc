/* Copyright (c) 2003 Heikki Orsila <heikki.orsila@tut.fi> */

#include <stdlib.h>
#include <stdio.h>

#include "event.h"

int event_append(struct event_queue *q, void *f, void *arg) {
  if (q->n >= q->max_events) {
    fprintf(stderr, "event queue full\n");
    return 0;
  }
  memset(&q->list[q->n], 0, sizeof(struct event));
  q->list[q->n].f = f;
  q->list[q->n].arg = arg;
  q->n++;
  return 1;
}

int event_init(struct event_queue *q, int max) {
  memset(q, 0, sizeof(struct event_queue));
  q->n = 0;
  q->max_events = max;
  q->list = malloc(sizeof(struct event) * max);
  if (!q->list) {
    fprintf(stderr, "no memory for event queue\n");
    return 0;
  }
  return 1;
}

void event_handler(struct event_queue *q) {
  void (*f)(void *arg);
  void *arg;
  while (q->n > 0) {
    q->n--;
    f = q->list[q->n].f;
    arg = q->list[q->n].arg;
    if (f) {
      f(arg); 
    } else {
      fprintf(stderr, "dummy event in event queue\n");
    }
  }
}
