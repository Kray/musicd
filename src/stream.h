#ifndef MUSICD_STREAM_H
#define MUSICD_STREAM_H

#include "format.h"
#include "libav.h"
#include "track.h"
#include "transcoder.h"

typedef struct stream {
  track_t *track;
  
  AVFormatContext *avctx;
  AVPacket avpacket;
  
  format_t src_format;

  transcoder_t *transcoder;
  uint8_t *buf;
  int buf_size;
  
  format_t *format;
  
  int at_end;
  
} stream_t;

stream_t *stream_open(track_t *track);
void stream_close(stream_t *stream);
int stream_set_transcoder(stream_t *stream, transcoder_t *transcoder);
uint8_t *stream_next(stream_t *stream, int *size, int64_t *pts);
int stream_seek(stream_t *stream, int position);




#endif
