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
