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
#ifndef MUSICD_LIBAV_H
#define MUSICD_LIBAV_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/opt.h>

#ifndef USE_AVRESAMPLE
  #include <libswresample/swresample.h>
  typedef SwrContext resampler_t;
  #define resampler_alloc swr_alloc
  #define resampler_init swr_init
  #define resampler_free swr_free
  #define resampler_convert swr_convert
#else
  #include <libavresample/avresample.h>
  typedef struct AVAudioResampleContext resampler_t;
  #define resampler_alloc avresample_alloc_context
  #define resampler_init avresample_open
  #define resampler_free avresample_free
  #define resampler_convert(resampler, out, out_count, in, in_count) \
    avresample_convert(resampler, out, 0, out_count, (uint8_t **)in, 0, in_count)
#endif


int musicd_av_lockmgr(void **mutex, enum AVLockOp operation);

/* Compatibility stuff for older versions of ffmpeg */

/*
 * CodecID -> AVCodecID appeared during 54
 */
#if LIBAVCODEC_VERSION_MAJOR < 54
# define USE_OLD_CODECID
#elif LIBAVCODEC_VERSION_MAJOR < 55
# ifndef CodecID
#  define USE_OLD_CODECID
# endif
#endif

#ifdef USE_OLD_CODECID
# define AVCodecID CodecID
# define AV_CODEC_ID_NONE CODEC_ID_NONE
# define AV_CODEC_ID_MP3 CODEC_ID_MP3
# define AV_CODEC_ID_VORBIS CODEC_ID_VORBIS
#endif

#if LIBAVCODEC_VERSION_MAJOR < 54
const char *avcodec_get_name(enum AVCodecID id);
#endif

#endif
