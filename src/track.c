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
#include "track.h"

#include "libav.h"
#include "log.h"

static char *get_av_dict_value(AVDictionary *dict, const char *key)
{
  AVDictionaryEntry *entry = av_dict_get(dict, key, NULL, 0);
  if (!entry) {
    return NULL;
  }
  return strdup(entry->value);
}

static char *copy_av_dict_value(AVDictionary *dict, const char *key)
{
  char *result = get_av_dict_value(dict, key);
  return result ? strdup(result) : NULL;
}

track_t *track_new()
{
  track_t *result = malloc(sizeof(track_t));
  memset(result, 0, sizeof(track_t));
  return result;
}

track_t *track_from_path(const char *path)
{
  AVFormatContext* avctx = NULL;
  track_t *track;
  char *tmp;
  
  /**
   * @todo TODO Own probing for ensuring probing score high enough to be sure
   * about file really being an audio file. */
  
  if (avformat_open_input(&avctx, path, NULL, NULL)) {
    return NULL;
  }
  
  if (avctx->nb_streams < 1
   || avctx->duration < 1) {
    if (av_find_stream_info(avctx) < 0) {
      av_close_input_file(avctx);
      return NULL;
    }
  }
  
  if (avctx->nb_streams < 1
   || avctx->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO
   || (avctx->duration && avctx->streams[0]->duration < 1)) {
    av_close_input_file(avctx);
    return NULL;
  }
  
  /*av_dump_format(avctx, 0, NULL, 0);*/
  
  /* For each metadata first try container-level metadata. If no value is
   * found, try stream-specific metadata.
   */
  
  track = track_new();
  
  track->path = strdup(path);
  
  tmp = get_av_dict_value(avctx->metadata, "track");
  if (!tmp) {
    tmp = get_av_dict_value(avctx->streams[0]->metadata, "track");
  }
  if (tmp) {
    sscanf(tmp, "%d", &track->track);
  }
  
  track->title = copy_av_dict_value(avctx->metadata, "title");
  if (!track->title) {
    track->title = copy_av_dict_value(avctx->streams[0]->metadata, "title");
  }
  
  track->artist = copy_av_dict_value(avctx->metadata, "artist");
  if (!track->artist) {
    track->artist = copy_av_dict_value(avctx->streams[0]->metadata, "artist");
  }
  
  track->album = copy_av_dict_value(avctx->metadata, "album");
  if (!track->album) {
    track->album = copy_av_dict_value(avctx->streams[0]->metadata, "album");
  }
  
  track->albumartist = copy_av_dict_value(avctx->metadata, "albumartist");
  if (!track->albumartist) {
    track->albumartist = copy_av_dict_value(avctx->streams[0]->metadata,
                                            "albumartist");
  }
  
  if (avctx->duration > 0) {
    track->duration = avctx->duration / (double)AV_TIME_BASE;
  } else {
    track->duration =
      avctx->streams[0]->duration * av_q2d(avctx->streams[0]->time_base);
  }
  
  /*musicd_log(LOG_DEBUG, "track",
    "track=%i title=%s artist=%s albumartist=%s albumartist=%s duration=%i",
    track->track, track->title, track->artist, track->album,
    track->albumartist, track->duration);*/
  
  av_close_input_file(avctx);
  return track;
}

void track_free(track_t *track)
{
  if (!track) {
    return;
  }
  
  free(track->path);
  free(track->title);
  free(track->artist);
  free(track->album);
  free(track->albumartist);
  
  free(track);
}

