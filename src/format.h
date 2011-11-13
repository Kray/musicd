#ifndef MUSICD_FORMAT_H
#define MUSICD_FORMAT_H

#include "libav.h"

typedef struct format {
  const char *codec;
  int samplerate;
  int bitspersample;
  int channels;
  const uint8_t *extradata;
  int extradata_size;
  
  /** Size of one raw audio frame in *bytes* encoder takes in. */
  int frame_size; 
} format_t;

/**
 * Fill all fields to @p dst from @p src excluding codec.
 */
void format_from_av(AVCodecContext *src, format_t *dst);

#endif