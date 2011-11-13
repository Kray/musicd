/*
 * This file is part of musicd.
 * Copyright (C) 2011 Konsta Kokkinen <kray@tsundere.fi>
 * 
 * Musicd is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * Musicd is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with Musicd.  If not, see <http://www.gnu.org/licenses/>.
 */
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
