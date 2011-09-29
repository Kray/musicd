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
#ifndef MUSICD_TRACK_H
#define MUSICD_TRACK_H

#include "libav.h"

typedef struct track {
  int id;
  
  char *url;
  
  char *name;
  char *artist;
  char *album;
  char *albumartist;
  
  int number;
  
  int duration;
  
  int start;
  
} track_t;


track_t *track_new();

track_t *track_from_url(const char *url);

void track_free(track_t *track);


typedef struct track_stream {
  track_t *track;
  AVFormatContext *avctx;
  AVPacket packet;
  int at_end;
  
  const char *codec;
  int samplerate;
  int bitspersample;
  int channels;
  const uint8_t *extradata;
  int extradata_size;
  
} track_stream_t;

track_stream_t *track_stream_open(track_t *track);
uint8_t *track_stream_read(track_stream_t *stream, int *size, int *pts);
int track_stream_seek(track_stream_t *stream, int position);
void track_stream_close(track_stream_t *stream);

#endif
