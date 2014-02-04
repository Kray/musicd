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
#ifndef MUSICD_FORMAT_H
#define MUSICD_FORMAT_H

#include "libav.h"

typedef enum codec_type {
  CODEC_TYPE_OTHER = -1,
  CODEC_TYPE_NONE = 0,
  CODEC_TYPE_MP3,
  CODEC_TYPE_OGG_VORBIS,
  CODEC_TYPE_FLAC,
  CODEC_TYPE_AAC,
  CODEC_TYPE_OPUS,
} codec_type_t;

codec_type_t codec_type_from_string(const char *string);


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
