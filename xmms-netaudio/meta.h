#ifndef _XMMS_NETAUDIO_META_H_
#define _XMMS_NETAUDIO_META_H_

#include <netinet/in.h>

typedef enum {
  FMT_U8, FMT_S8, FMT_U16_LE, FMT_U16_BE, FMT_U16_NE, FMT_S16_LE, FMT_S16_BE, FMT_S16_NE
} na_format_t;

struct na_meta {
  na_format_t fmt;
  uint32_t rate;
  uint32_t nch;
};

#endif
