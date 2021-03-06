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
      /* notice that f() may add events to the queue */
      f(arg);
    } else {
      fprintf(stderr, "dummy event in event queue\n");
    }
  }
}
