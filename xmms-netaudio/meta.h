#ifndef _XMMS_NETAUDIO_META_H_
#define _XMMS_NETAUDIO_META_H_

#include <netinet/in.h>

typedef enum {
  NA_FMT_U8, NA_FMT_S8, NA_FMT_U16_LE, NA_FMT_U16_BE, NA_FMT_U16_NE, NA_FMT_S16_LE, NA_FMT_S16_BE, NA_FMT_S16_NE
} na_format_t;

struct na_meta {
  na_format_t fmt;
  uint32_t rate;
  uint32_t nch;
};

#endif
