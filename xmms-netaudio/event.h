#ifndef _XMMS_NETAUDIO_EVENT_H_
#define _XMMS_NETAUDIO_EVENT_H_

struct event {
  void *f;
  void *arg;
};

struct event_queue {
  int max_events;
  int n;
  struct event *list;
};

void event_handler(struct event_queue *q);
int event_append(struct event_queue *q, void *f, void *arg);
int event_init(struct event_queue *q, int max);

#endif
