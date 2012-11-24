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
#include "transcoder.h"

#include "libav.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>

transcoder_t *transcoder_open(format_t *format, const char *codec, int bitrate)
{
  int result;
  AVCodec *codec_in, *codec_out;
  AVCodecContext *decoder;
  AVCodecContext *encoder;
  
  transcoder_t *transcoder;
  
  if (!format || !codec) {
    return NULL;
  }
  
  if (strcmp(codec, "mp3")) {
    musicd_log(LOG_ERROR, "transcoder", "unsupported codec '%s' requested",
               codec);
    return NULL;
  }
  
  codec_in = avcodec_find_decoder_by_name(format->codec);
  if (!codec_in) {
    musicd_log(LOG_ERROR, "transcoder", "decoder not found");
    return NULL;
  }
  
  codec_out = avcodec_find_encoder(CODEC_ID_MP3);
  if (!codec_out) {
    musicd_log(LOG_ERROR, "transcoder", "requested encoder not found");
    return NULL;
  }
  
  decoder = avcodec_alloc_context3(NULL);
  encoder = avcodec_alloc_context3(NULL);
  
  decoder->sample_rate = format->samplerate;
  decoder->channels = format->channels;
  decoder->extradata = (uint8_t *)format->extradata;
  decoder->extradata_size = format->extradata_size;
  
  result = avcodec_open2(decoder, codec_in, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "transcoder", "could not open decoder: %s",
               strerror(AVUNERROR(result)));
    goto fail;
  }
  
  encoder->sample_rate = format->samplerate;
  /** @todo FIXME This won't work in all cases. */
  encoder->sample_fmt = decoder->sample_fmt;
  
  encoder->channels = format->channels;
  
  /** @todo FIXME Hard-coded values. */
  if (bitrate < 64000 || bitrate > 320000) {
    bitrate = 128000;
  }
  
  encoder->bit_rate = bitrate;
  
  result = avcodec_open2(encoder, codec_out, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "transcoder", "could not open encoder: %s",
               strerror(AVUNERROR(result)));
    avcodec_close(decoder);
    goto fail;
  }
  
  transcoder = malloc(sizeof(transcoder_t));
  memset(transcoder, 0, sizeof(transcoder_t));
  
  transcoder->decoder = decoder;
  transcoder->encoder = encoder;
  
  format_from_av(encoder, &transcoder->format);
  transcoder->format.codec = codec;
    
  av_init_packet(&transcoder->avpacket);
  
  transcoder->packet = malloc(FF_MIN_BUFFER_SIZE);
  
  return transcoder;

fail:
  av_free(decoder);
  av_free(encoder);
  
  return NULL;
}

void transcoder_close(transcoder_t* transcoder)
{
  if (!transcoder) {
    return;
  }
  
  avcodec_close(transcoder->decoder);
  avcodec_close(transcoder->encoder);
  av_free(transcoder->decoder);
  av_free(transcoder->encoder);
  free(transcoder->packet);
  free(transcoder);
}

static int decode_next(transcoder_t *transcoder, uint8_t *data, int src_size)
{ 
  int result;
  int16_t samples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
  int size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
  
  transcoder->avpacket.data = data;
  transcoder->avpacket.size = src_size;
    
  result = avcodec_decode_audio3(transcoder->decoder, samples, &size,
                                 &transcoder->avpacket);
  if (result < 0) {
    /* For some reason decoding certain files in certain situations might fail
     * for a few packets. */
    ++transcoder->error_counter;
    musicd_log(LOG_VERBOSE, "transcoder", "can't decode: %s, error_counter = %d",
               strerror(AVUNERROR(result)), transcoder->error_counter);
    if (transcoder->error_counter > 5) {
      musicd_log(LOG_ERROR, "transcoder", "error_counter too high, failing: %s",
                 strerror(AVUNERROR(result)));
      return -1;
    }
    return 0;
  }
  if (transcoder->error_counter) {
    musicd_log(LOG_VERBOSE, "transcoder", "recovered from error_counter = %d",
               transcoder->error_counter);
    transcoder->error_counter = 0;
  }

  transcoder->buf =
    realloc(transcoder->buf, transcoder->buf_size + size);
  
  memcpy(transcoder->buf + transcoder->buf_size, samples, size);
  transcoder->buf_size += size;
  
  return 0;
}

static int encode_next(transcoder_t *transcoder)
{
  int result;

  if (transcoder->buf_size < transcoder->format.frame_size) {
    return 0;
  }
  
  result = avcodec_encode_audio(transcoder->encoder, transcoder->packet,
                                FF_MIN_BUFFER_SIZE,
                                (int16_t *)transcoder->buf);
  if (result < 0) {
    musicd_log(LOG_ERROR, "transcoder", "could not encode: %s",
               strerror(AVUNERROR(result)));
    return -1;
  }
  
  if (result == 0) {
    return 0;
  }
  
  memmove(transcoder->buf, transcoder->buf + transcoder->format.frame_size,
          transcoder->buf_size - transcoder->format.frame_size);
  transcoder->buf_size -= transcoder->format.frame_size;

  transcoder->packet_size = result;

  return result;
}
int transcoder_transcode(transcoder_t *transcoder, uint8_t *src, int size)
{
  if (src) {
    if (decode_next(transcoder, src, size)) {
      return -1;
    }
  }
  
  return encode_next(transcoder);
}

