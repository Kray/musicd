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
#include "stream.h"

#include "log.h"
#include "strings.h"

static double dict_to_double(AVDictionary *dict, const char *key)
{
  AVDictionaryEntry *entry;
  double result = 0.0;
  entry = av_dict_get(dict, key, NULL, 0);
  if (!entry) {
    return 0.0;
  }
  sscanf(entry->value, "%lf", &result);
  return result;
}
static void find_replay_gain(stream_t *stream, AVDictionary *dict)
{
  stream->replay_track_gain = dict_to_double(dict, "REPLAYGAIN_TRACK_GAIN");
  stream->replay_album_gain = dict_to_double(dict, "REPLAYGAIN_ALBUM_GAIN");
  stream->replay_track_peak = dict_to_double(dict, "REPLAYGAIN_TRACK_PEAK");
  stream->replay_album_peak = dict_to_double(dict, "REPLAYGAIN_ALBUM_PEAK");
}

stream_t *stream_new()
{
  stream_t *stream = malloc(sizeof(stream_t));
  memset(stream, 0, sizeof(stream_t));
  av_init_packet(&stream->src_packet);
  return stream;
}

void stream_close(stream_t *stream)
{
  if (!stream) {
    return;
  }

  track_free(stream->track);
  free(stream->src_buf);
  free(stream->dst_data);
  av_free(stream->dst_iobuf);
  av_free(stream->dst_ioctx);

  if (stream->src_ctx) {
    avformat_close_input(&stream->src_ctx);
  }
  if (stream->dst_ctx) {
    avformat_free_context(stream->dst_ctx);
  }
  
  if (stream->decoder) {
    avcodec_close(stream->decoder);
    av_free(stream->decoder);
  }
  if (stream->encoder) {
    avcodec_close(stream->encoder);
    av_free(stream->encoder);
  }

  free(stream);
}

bool stream_open(stream_t *stream, track_t* track)
{
  int result;
  AVFormatContext *ctx = NULL;
  AVCodec *src_codec;
  
  if (!track) {
    return false;
  }

  result = avformat_open_input(&ctx, track->file, NULL, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream", "can't open file '%s': %s",
               track->file, strerror(AVUNERROR(result)));
    return false;
  }

  result = avformat_find_stream_info(ctx, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream", "can't find stream info of '%s': %s",
               track->file, strerror(AVUNERROR(result)));
    avformat_close_input(&ctx);
    return false;
  }

  if (ctx->nb_streams < 1
   || ctx->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO
   || ctx->streams[0]->codec->codec_id == AV_CODEC_ID_NONE
   || (ctx->duration < 1 && ctx->streams[0]->duration < 1)) {
    avformat_close_input(&ctx);
    return false;
  }

  src_codec = avcodec_find_decoder(ctx->streams[0]->codec->codec_id);
  if (!src_codec) {
    musicd_log(LOG_ERROR, "stream", "decoder not found");
    avformat_close_input(&ctx);
    return false;
  }

  stream->track = track;
  stream->src_ctx = ctx;
  stream->src_codec = src_codec;
  if (stream->src_codec->id == AV_CODEC_ID_MP3) {
    stream->src_codec_type = CODEC_TYPE_MP3;
  } else if (stream->src_codec->id == AV_CODEC_ID_VORBIS) {
    stream->src_codec_type = CODEC_TYPE_OGG_VORBIS;
  } else {
    stream->src_codec_type = CODEC_TYPE_OTHER;
  }

  format_from_av(stream->src_ctx->streams[0]->codec, &stream->format);

  /* Replay gain, test container metadata, then stream metadata. */
  find_replay_gain(stream, stream->src_ctx->metadata);
  if (stream->replay_track_gain == 0.0 && stream->replay_track_gain == 0.0) {
    find_replay_gain(stream, stream->src_ctx->streams[0]->metadata);
  }
  musicd_log(LOG_DEBUG, "stream", "replaygain: %f %f %f %f",
             stream->replay_track_gain, stream->replay_album_gain,
             stream->replay_track_peak, stream->replay_album_peak);

  /* If track does not begin from the beginning of the file, seek to relative
   * zero. */
  if (stream->track->start > 0) {
    stream_seek(stream, 0);
  }
  return true;
}

bool stream_transcode(stream_t *stream, codec_type_t codec_type, int bitrate)
{
  int result;
  enum AVCodecID dst_codec_id;
  AVCodec *dst_codec;
  AVCodecContext *decoder;
  AVCodecContext *encoder;

  if (codec_type == CODEC_TYPE_MP3) {
    dst_codec_id = AV_CODEC_ID_MP3;
  } else if (codec_type == CODEC_TYPE_OGG_VORBIS) {
    dst_codec_id = AV_CODEC_ID_VORBIS;
  } else {
    musicd_log(LOG_ERROR, "stream", "unsupported encoder requested");
    return false;;
  }

  dst_codec = avcodec_find_encoder(dst_codec_id);
  if (!dst_codec) {
    musicd_log(LOG_ERROR, "stream", "requested encoder not found");
    return false;
  }

  decoder = avcodec_alloc_context3(stream->src_codec);
  avcodec_copy_context(decoder, stream->src_ctx->streams[0]->codec);

  result = avcodec_open2(decoder, stream->src_codec, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream", "can't open decoder: %s",
               strerror(AVUNERROR(result)));
    av_free(decoder);
    return false;
  }

  /** @todo FIXME Hard-coded values. */
  if (bitrate < 64000 || bitrate > 320000) {
    bitrate = 196000;
  }

  encoder = avcodec_alloc_context3(dst_codec);
  encoder->sample_rate = decoder->sample_rate;
  encoder->channels = decoder->channels;
  encoder->sample_fmt = decoder->sample_fmt;
  encoder->channel_layout = decoder->channel_layout;
  encoder->bit_rate = bitrate;

  result = avcodec_open2(encoder, dst_codec, NULL);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream", "can't open encoder: %s",
               strerror(AVUNERROR(result)));
    avcodec_close(decoder);
    av_free(decoder);
    av_free(encoder);
    return false;
  }


  stream->decoder = decoder;
  stream->encoder = encoder;
  stream->dst_codec = dst_codec;
  stream->dst_codec_type = codec_type;
  format_from_av(encoder, &stream->format);

  stream->dst_data = malloc(FF_MIN_BUFFER_SIZE);
  return true;
}

bool stream_remux(stream_t *stream,
                   int (*write)(void *opaque, uint8_t *buf, int buf_size),
                   void *opaque)
{
  const char *format_name;
  AVOutputFormat *dst_format;
  AVFormatContext *dst_ctx;
  AVStream *dst_stream;
  uint8_t *dst_iobuf;
  AVIOContext *dst_ioctx;

  if (stream->dst_codec_type == CODEC_TYPE_MP3) {
    format_name = "mp3";
  } else if (stream->dst_codec_type == CODEC_TYPE_OGG_VORBIS) {
    format_name = "ogg";
  } else {
    return false;
  }

  dst_format = av_guess_format(format_name, NULL, NULL);
  if (!dst_format) {
    musicd_log(LOG_ERROR, "stream", "can't find encoder for %s",format_name);
    return false;
  }

  dst_ctx = avformat_alloc_context();
  dst_ctx->oformat = dst_format;

  dst_stream = avformat_new_stream(dst_ctx, stream->dst_codec);
  if (!dst_stream) {
    musicd_log(LOG_ERROR, "stream", "avformat_new_stream failed");
    avformat_free_context(dst_ctx);
    return false;
  }

  av_dict_set(&dst_ctx->metadata, "track",
              stringf("%02d", stream->track->track), AV_DICT_DONT_STRDUP_VAL);
  av_dict_set(&dst_ctx->metadata, "title", stream->track->title, 0);
  av_dict_set(&dst_ctx->metadata, "artist", stream->track->artist, 0);
  av_dict_set(&dst_ctx->metadata, "album", stream->track->album, 0);
  
  dst_stream->codec = avcodec_alloc_context3(NULL);
  avcodec_copy_context(dst_stream->codec, stream->encoder);

  dst_iobuf = av_mallocz(4096);
  dst_ioctx =
    avio_alloc_context(dst_iobuf, 4096, 1, opaque, NULL, write, NULL);
  if (!dst_ioctx) {
    musicd_log(LOG_ERROR, "stream", "avio_alloc_context failed");
    av_free(dst_iobuf);
    avformat_free_context(dst_ctx);
    return false;
  }
  dst_ctx->pb = dst_ioctx;
  stream->dst_ctx = dst_ctx;
  stream->dst_iobuf = dst_iobuf;
  stream->dst_ioctx = dst_ioctx;

  return true;
}

int stream_start(stream_t *stream)
{
  if (stream->dst_ctx) {
    avformat_write_header(stream->dst_ctx, NULL);
  }
  return 0;
}

static int read_next(stream_t *stream)
{
  int result;
  
  av_free_packet(&stream->src_packet);
  
  while (1) {
    result = av_read_frame(stream->src_ctx, &stream->src_packet);
    if (result < 0) {
      if (result == AVERROR_EOF) {
        /* end of file */
        return 0;
      }
      musicd_log(LOG_ERROR, "stream", "av_read_frame failed: %s",
                 strerror(AVUNERROR(result)));
      return 0;
    }

    if (stream->src_packet.stream_index != 0) {
      continue;
    }
    break;
  }

  if (stream->src_packet.pts * av_q2d(stream->src_ctx->streams[0]->time_base) >
      stream->track->start + stream->track->duration) {
    if (stream->track->cuefile) {
      /* Accurate end of track */
      return 0;
    } else {
      /* Miscalculated track length */
    }
  }
  
  return 1;
}

static int decode_next(stream_t *stream)
{
  int result;
  int16_t samples[AVCODEC_MAX_AUDIO_FRAME_SIZE];
  int size = AVCODEC_MAX_AUDIO_FRAME_SIZE;
  
  result = read_next(stream);
  if (result <= 0) {
    return result;
  }
    
  result = avcodec_decode_audio3(stream->decoder, samples, &size,
                                 &stream->src_packet);
  if (result < 0) {
    /* Decoding right after seeking, especially with mp3, might fail because
     * we don't have the frame header available yet. */
    ++stream->error_counter;
    musicd_log(LOG_VERBOSE, "stream", "can't decode: %s, error_counter = %d",
               strerror(AVUNERROR(result)), stream->error_counter);
    if (stream->error_counter > 10) {
      musicd_log(LOG_ERROR, "stream", "error_counter too high, failing: %s",
                 strerror(AVUNERROR(result)));
      return -1;
    }
    /* Technically we failed, but we are going to try again so make it look
     * like success... */
    return 1;
  }
  if (stream->error_counter) {
    musicd_log(LOG_VERBOSE, "stream", "recovered from error_counter = %d",
               stream->error_counter);
    stream->error_counter = 0;
  }

  /* Grow buffer if too small */
  if (stream->src_buf_space < stream->src_buf_size + size) {
    stream->src_buf = realloc(stream->src_buf, stream->src_buf_size + size);
    stream->src_buf_space = stream->src_buf_size + size;
  }

  memcpy(stream->src_buf + stream->src_buf_size, samples, size);
  stream->src_buf_size += size;
  
  return 1;
}

static int encode_next(stream_t *stream)
{
  int result;

  while (stream->src_buf_size < stream->format.frame_size) {

    result = decode_next(stream);
    if (result <= 0) {
      return result;
    }
  }

  result = avcodec_encode_audio(stream->encoder, stream->dst_data,
                                FF_MIN_BUFFER_SIZE,
                                (int16_t *)stream->src_buf);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream", "can't encode: %s",
               strerror(AVUNERROR(result)));
    return -1;
  }

  memmove(stream->src_buf, stream->src_buf + stream->format.frame_size,
          stream->src_buf_size - stream->format.frame_size);
  stream->src_buf_size -= stream->format.frame_size;

  stream->pts += stream->format.frame_size / (stream->format.channels *
                 (stream->format.bitspersample / 8)) * AV_TIME_BASE /
                 stream->format.samplerate;

  stream->dst_size = result;

  return 1;
}

static int get_next(stream_t *stream)
{
  int result;
  if (stream->dst_codec_type) {
    do {
      result = encode_next(stream);
      if (result <= 0) {
        return result;
      }
    } while (stream->dst_size == 0);

    stream->data = stream->dst_data;
    stream->size = stream->dst_size;

  } else {
    result = read_next(stream);
    stream->data = stream->src_packet.data;
    stream->size = stream->src_packet.size;
  }
  return result;
}

static int mux_next(stream_t *stream)
{
  int result;
  AVPacket packet;

  do {
    result = get_next(stream);
    if (result <= 0) {
      return result;
    }
  } while (stream->dst_size == 0);

  av_init_packet(&packet);
  packet.data = stream->dst_data;
  packet.size = stream->dst_size;
  /*packet.pts = stream->pts;*/ /* FIXME: proper PTS/DTS handling */
  packet.stream_index = 0;

  result = av_interleaved_write_frame(stream->dst_ctx, &packet);
  if (result < 0) {
    musicd_log(LOG_ERROR, "stream",
               "av_interleaved_write_frame failed: %s",
               strerror(AVUNERROR(result)));
    return -1;
  }

  return 1;
}

int stream_next(stream_t *stream)
{
  if (stream->dst_ctx) {
    return mux_next(stream);
  }
  return get_next(stream);
}

bool stream_seek(stream_t *stream, double position)
{
  bool result;
  int64_t seek_pos;
  
  seek_pos = (position + stream->track->start) /
    av_q2d(stream->src_ctx->streams[0]->time_base);
  
  result = av_seek_frame(stream->src_ctx, 0, seek_pos, 0);
  
  stream->pts = position * AV_TIME_BASE;

  return result >= 0 ? true : false;
}
