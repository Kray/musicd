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
#include "libav.h"


#if LIBAVCODEC_VERSION_MAJOR < 54
/* Modified code from ffmpeg-0.11.2 */
const char *avcodec_get_name(enum AVCodecID id)
{
  AVCodec *codec;

  codec = avcodec_find_decoder(id);
  if (codec) {
    return codec->name;
  }
  codec = avcodec_find_encoder(id);
  if (codec) {
    return codec->name;
  }
  return "unknown_codec";
}
#endif
