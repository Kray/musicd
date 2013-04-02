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

#include <stdbool.h>

typedef struct stream {
  track_t *track;

  /*** read & demux ***/
  AVFormatContext *src_ctx;
  const AVCodec *src_codec;
  codec_type_t src_codec_type;

  AVPacket src_packet;


  /*** transcode ***/
  /* decode */
  AVFrame *decode_frame;
  AVCodecContext *decoder;

  /* resample */
  AVAudioResampleContext *resampler;
  AVFrame *resample_frame;
  uint8_t *resample_buf;

  /* buffer */
  AVAudioFifo *src_buf;


  /* encode */
  AVFrame *encode_frame;
  uint8_t *encode_buf;
  AVCodecContext *encoder;
  AVCodec *dst_codec;
  codec_type_t dst_codec_type;
  int error_counter;

  AVPacket encode_packet;


  /*** result packet */
  uint8_t *dst_data;
  int dst_size;


  /*** remux ***/
  AVFormatContext *dst_ctx;
  uint8_t *dst_iobuf;
  AVIOContext *dst_ioctx;


  /*** result ***/
  format_t format;
  double replay_track_gain;
  double replay_album_gain;
  double replay_track_peak;
  double replay_album_peak;

  /* ready packet after successful stream_next */
  uint8_t *data;
  int size;
  int64_t pts;

} stream_t;

stream_t *stream_new();
void stream_close(stream_t *stream);

/**
 * Starts reading @p track
 */
bool stream_open(stream_t *stream, track_t *track);
/**
 * Starts transcoding to @p codec at @p bitrate bps
 */
bool stream_transcode(stream_t *stream, codec_type_t codec, int bitrate);
/**
 * Starts remuxing @p stream
 * @p write callback function that processes data written
 */
bool stream_remux(stream_t *stream,
                   int (*write)(void *opaque, uint8_t *buf, int buf_size),
                   void *opaque);

int stream_start(stream_t *stream);

/**
 * Handles next packet.
 * Data can be retrieved from stream->data and stream->size.
 * If stream_remux was successfully called, write function was called to feed
 * the data.
 * @returns >0 if success, 0 if EOF, <0 on error
 */
int stream_next(stream_t *stream);


/**
 * Seeks to absolute @p position seconds
 */
bool stream_seek(stream_t *stream, double position);

#endif
