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
#include "format.h"

codec_type_t codec_type_from_string(const char *string)
{
  if (!string || strlen(string) == 0) {
    return CODEC_TYPE_NONE;
  }
  if (!strcmp(string, "mp3")) {
    return CODEC_TYPE_MP3;
  }
  if (!strcmp(string, "ogg") || !strcmp(string, "vorbis")) {
    return CODEC_TYPE_OGG_VORBIS;
  }
  if (!strcmp(string, "flac")) {
    return CODEC_TYPE_FLAC;
  }
  if (!strcmp(string, "aac")) {
    return CODEC_TYPE_AAC;
  }
  if (!strcmp(string, "opus")) {
    return CODEC_TYPE_OPUS;
  }
  return CODEC_TYPE_OTHER;
}

void format_from_av(AVCodecContext *src, format_t *dst)
{
  dst->codec = avcodec_get_name(src->codec_id);
  dst->samplerate = src->sample_rate;
  dst->bitspersample =  av_get_bytes_per_sample(src->sample_fmt) * 8;
  dst->channels = src->channels;
  dst->extradata = src->extradata;
  dst->extradata_size = src->extradata_size;
  dst->frame_size =
    src->frame_size * src->channels * av_get_bytes_per_sample(src->sample_fmt);
}
