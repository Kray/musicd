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

stream_t *stream_open(track_t *track)
{
  AVFormatContext *avctx = NULL;
  stream_t *stream;
  AVCodec *codec;
  
  if (avformat_open_input(&avctx, track->path, NULL, NULL)) {
    musicd_log(LOG_ERROR, "stream", "Could not open file '%s'", track->path);
    return NULL;
  }
  
  if (av_find_stream_info(avctx) < 0) {
    av_close_input_file(avctx);
    return NULL;
  }
  
  if (avctx->nb_streams < 1
   || avctx->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO
   || (avctx->duration < 1 && avctx->streams[0]->duration < 1)) {
    av_close_input_file(avctx);
    return NULL;
  }
  
  codec = avcodec_find_decoder(avctx->streams[0]->codec->codec_id);
  if (!codec) {
    av_close_input_file(avctx);
    return NULL;
  }
  
  stream = malloc(sizeof(stream_t));
  memset(stream, 0, sizeof(stream_t));
  
  stream->track = track;
  
  stream->avctx = avctx;
  av_init_packet(&stream->avpacket);

  stream->src_format.codec = codec->name;
  
  format_from_av(stream->avctx->streams[0]->codec, &stream->src_format);
  
  stream->format = &stream->src_format;
  
  /* Replay gain, test container metadata, then stream metadata. */
  find_replay_gain(stream, avctx->metadata);
  if (stream->replay_track_gain == 0.0 && stream->replay_track_gain == 0.0) {
    find_replay_gain(stream, avctx->streams[0]->metadata);
  }
  musicd_log(LOG_DEBUG, "stream", "replaygain: %f %f %f %f",
             stream->replay_track_gain, stream->replay_album_gain,
             stream->replay_track_peak, stream->replay_album_peak);
  
  /* If track does not begin from the beginning of the file, seek to relative
   * zero. */
  if (stream->track->start > 0) {
    stream_seek(stream, 0);
  }

  return stream;
}

void stream_close(stream_t *stream)
{
  if (!stream) {
    return;
  }
  transcoder_close(stream->transcoder);
  av_close_input_file(stream->avctx);
  free(stream);
}

int stream_set_transcoder(stream_t* stream, transcoder_t *transcoder)
{
  if (stream->transcoder) {
    transcoder_close(stream->transcoder);
    stream->format = &stream->src_format;
  }
  
  stream->transcoder = transcoder;
  if (stream->transcoder) {
    stream->format = &transcoder->format;
  }
  return 0;
}

static int read_next(stream_t *stream)
{
  int result;
  
  av_free_packet(&stream->avpacket);
  
  while (1) {
    result = av_read_frame(stream->avctx, &stream->avpacket);
    if (result < 0) {
      musicd_log(LOG_WARNING, "stream", "av_read_frame() < 0, FIXME");
      stream->at_end = 1;
      return -1;
    }

    if (stream->avpacket.stream_index != 0) {
      continue;
    }
    break;
  }
  
  if (floor(stream->avpacket.pts * av_q2d(stream->avctx->streams[0]->time_base))
      >= stream->track->start + stream->track->duration) {
    stream->at_end = 1;
    return -1;
  }
  
  return 0;
}

uint8_t* stream_next(stream_t *stream, int *size, int64_t *pts)
{
  int result = 0;
  
  if (stream->transcoder) {
    result = transcoder_transcode(stream->transcoder, NULL, 0);
    if (result > 0) {
      *size = stream->transcoder->packet_size;
      return stream->transcoder->packet;
    }
    
    if (result < 0) {
      return NULL;
    }
    
    while (result == 0) {
      if (read_next(stream)) {
        return NULL;
      }
      result = transcoder_transcode(stream->transcoder, stream->avpacket.data,
                                    stream->avpacket.size);
    }
    if (result < 0) {
      return NULL;
    }
    *size = stream->transcoder->packet_size;
    return stream->transcoder->packet;
  } else {
  
    if (read_next(stream)) {
      return NULL;
    }
    
    *size = stream->avpacket.size;
    *pts = (stream->avpacket.pts * av_q2d(stream->avctx->streams[0]->time_base) *
      AV_TIME_BASE) - (stream->track->start * AV_TIME_BASE);

    return stream->avpacket.data;
  }
}

int stream_seek(stream_t *stream, int position)
{
  int result;
  int64_t seek_pos;
  
  seek_pos = (position + stream->track->start) /
    av_q2d(stream->avctx->streams[0]->time_base);
  
  result = av_seek_frame(stream->avctx, 0, seek_pos, 0);
  
  stream->at_end = 0;
  
  return result;
}

