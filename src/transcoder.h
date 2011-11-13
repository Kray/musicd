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
#ifndef MUSICD_TRANSCODER_H
#define MUSICD_TRANSCODER_H

#include "format.h"
#include "libav.h"
#include "track.h"

typedef struct transcoder {
  
  AVCodecContext *decoder;
  AVPacket avpacket;
  AVCodecContext *encoder;
  
  format_t format;
  
  uint8_t *buf;
  int buf_size;
  
  uint8_t *packet;
  int packet_size;
} transcoder_t;


transcoder_t *
  transcoder_open(format_t *format, const char *codec, int bitrate);
void transcoder_close(transcoder_t *transcoder);

/**
 * Packet can be retrieved from transcoder->packet and transcoder->packet_size.
 * @note Should be first called with @p src set to NULL to check if there is
 * data buffered.
 * @param src Source packet data or NULL.
 * @param size Source packet size.
 * @returns Less than 0 on failure, 0 if no packet packet was encoded
 * (=insufficient source data) or greater than zero.
 */
int transcoder_transcode(transcoder_t *transcoder, uint8_t *src, int size);


#endif
