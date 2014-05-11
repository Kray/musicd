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
#include "strings.h"

/* First try container-level metadata. If no value is found, try
 * stream-specific metadata.
 */
static char *get_metadata(AVFormatContext *avctx, const char *key)
{
  AVDictionaryEntry *entry = av_dict_get(avctx->metadata, key, NULL, 0);
  if (!entry) {
    entry = av_dict_get(avctx->streams[0]->metadata, key, NULL, 0);
    if (!entry) {
      return NULL;
    }
  }
  return entry->value;
}

static char *copy_metadata(AVFormatContext *avctx, const char *key)
{
  char *result = get_metadata(avctx, key);
  return strcopy(result);
}

track_t *track_new()
{
  track_t *result = malloc(sizeof(track_t));
  memset(result, 0, sizeof(track_t));
  return result;
}

int is_valid_audio_file(AVFormatContext *avctx)
{
  /**
   * @todo TODO Own probing for ensuring probing score high enough to be sure
   * about the file really being an audio file. */

  if (avctx->nb_streams < 1 || avctx->duration < 1) {
    if (avformat_find_stream_info(avctx, NULL) < 0) {
      avformat_close_input(&avctx);
      return 0;
    }
  }

  if (avctx->nb_streams < 1
      || avctx->streams[0]->codec->codec_type != AVMEDIA_TYPE_AUDIO
      || (avctx->duration && avctx->streams[0]->duration < 1)) {
    avformat_close_input(&avctx);
    return 0;
  }
  return 1;
}

void tracks_free(track_t **tracks)
{
  int i;
  for (i = 0; tracks[i] != NULL; ++i) {
    track_free(tracks[i]);
  }
  free(tracks);
}

static track_t *track_create(const char *path, AVFormatContext *avctx, 
                             int track_index)
{
  track_t *track;
  char *tmp;

  /*av_dump_format(avctx, 0, NULL, 0);*/

  /* For each metadata first try container-level metadata. If no value is
   * found, try stream-specific metadata.
   */

  track = track_new();

  track->file = strcopy(path);

  track->title = copy_metadata(avctx, "title") ? : copy_metadata(avctx, "song");

  if (!track->title) {
    /* No title in metadata, use plain filename (no basename, it's crap). */
    for (tmp = (char *)path + strlen(path);
        tmp > path && *(tmp - 1) != '/';
        --tmp) { }
    track->title = strcopy(tmp);
  }

  track->track_index = track_index;

  tmp = get_metadata(avctx, "track");
  if (tmp) {
    sscanf(tmp, "%d", &track->track);
  } else {
    track->track = track_index;
  }

  track->artist = copy_metadata(avctx, "artist") ? : copy_metadata(avctx, "author");
  track->album = copy_metadata(avctx, "album") ? : copy_metadata(avctx, "game");
  track->albumartist = copy_metadata(avctx, "albumartist");

  if (avctx->duration > 0) {
    track->duration = avctx->duration / (double)AV_TIME_BASE;
  } else {
    track->duration =
      avctx->streams[0]->duration * av_q2d(avctx->streams[0]->time_base);
  }

  if (track->duration <= 0) {
    track_free(track);
    track = NULL;
  }
  return track;
}

track_t **tracks_from_path(const char *path)
{
  AVFormatContext *avctx = NULL;
  track_t **tracks; /* NULL terminated */
  int track_count;
  int i;
  char *tmp;

  if (avformat_open_input(&avctx, path, NULL, NULL)) {
    return NULL;
  }
  if (!is_valid_audio_file(avctx)) {
    return NULL;
  }

  tmp = copy_metadata(avctx, "tracks");
  if (tmp) {
    sscanf(tmp, "%d", &track_count);
  } else {
    track_count = 1;
  }

  tracks = calloc(track_count + 1, sizeof(track_t *));

  for (i = 0; i < track_count; ++i) {
    tracks[i] = track_create(path, avctx, i);
  }

  avformat_close_input(&avctx);
  return tracks;
}

track_t *track_from_path(const char *path)
{
  AVFormatContext *avctx = NULL;
  track_t *track;
  
  if (avformat_open_input(&avctx, path, NULL, NULL)) {
    return NULL;
  }
  if (!is_valid_audio_file(avctx)) {
    return NULL;
  }
  
  track = track_create(path, avctx, -1);
 
  avformat_close_input(&avctx);
  return track;
}

void track_free(track_t *track)
{
  if (!track) {
    return;
  }
  
  free(track->file);
  free(track->cuefile);
  free(track->title);
  free(track->artist);
  free(track->album);
  free(track->albumartist);
  
  free(track);
}

