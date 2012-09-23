/*
 * This file is part of musicd.
 * Copyright (C) 2011-2012 Konsta Kokkinen <kray@tsundere.fi>
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

#include "protocol_musicd.h"

#include "cache.h"
#include "client.h"
#include "config.h"
#include "image.h"
#include "library.h"
#include "log.h"
#include "lyrics.h"
#include "server.h"
#include "strings.h"
#include "task.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct self {
  client_t *client;
  char *user;
  stream_t *stream;
} self_t;

static void send_track(client_t *client, track_t* track)
{
  client_send(client, "track\n");
  client_send(client, "id=%lu\n", track->id);
  client_send(client, "path=%s\n", track->path);
  client_send(client, "track=%i\n", track->track);
  client_send(client, "title=%s\n", track->title);
  client_send(client, "artistid=%lu\n", track->artistid);
  client_send(client, "artist=%s\n", track->artist);
  client_send(client, "albumid=%lu\n", track->albumid);
  client_send(client, "album=%s\n", track->album);
  client_send(client, "duration=%i\n", track->duration);
  client_send(client, "\n");
}

static char *line_read(char **string)
{
  char *result, *begin = *string;
  for (; **string != '\n' && **string != '\0'; ++(*string)) { }
  result = malloc(*string - begin + 1);
  strncpy(result, begin, *string - begin);
  result[*string - begin] = '\0';
  ++*string;
  return result;
}

static char *get_str(char *src, const char *key)
{
  char *end, *result;
  char search[strlen(key) + 2];
  snprintf(search, strlen(key) + 2, "%s=", key);
  
  for (; *src != '\n' && *src != '\0';) {
    if (!strncmp(src, search, strlen(key) + 1)) {

      for (; *src != '='; ++src) { }
      ++src;
      for (end = src; *end != '\n' && *end != '\0'; ++end) { }

      result = malloc(end - src + 1);
      strncpy(result, src, end - src);
      result[end - src] = '\0';

      return result;
    }
    
    for (; *src != '\n' && (*src + 1) != '\0'; ++src) { }
    ++src;
  }
  return NULL;
}

static int64_t get_int(char *src, const char *key)
{
  int64_t result = 0;
  char *search;
  search = stringf("%s=%%ld", key);
  for (; *src != '\n' && *src != '\0';) {
    if (sscanf(src, search, &result)) {
      break;
    }
    for (; *src != '\n' && (*src + 1) != '\0'; ++src) { }
    ++src;
  }
  free(search);
  return result;
}

static int client_error(client_t *client, const char *code)
{
  client_send(client, "error\nname=%s\n\n", code);
  return 0;
}


static int method_musicd(self_t *self, char *p)
{
  (void)p;
  client_send(self->client, "musicd\nprotocol=3\ncodecs=mp3\n\n");
  return 0;
}


static int method_auth(self_t *self, char *p)
{
  char *user, *pass;
  
  user = get_str(p, "user");
  pass = get_str(p, "password");
  
  /*musicd_log(LOG_DEBUG, "protocol_musicd", "%s %s", user, pass);*/
  
  if (!user || strcmp(user, config_get("user"))
   || !pass || strcmp(pass, config_get("password"))) {
    free(user);
    free(pass);
    return client_error(self->client, "invalid_login");
  }
  
  self->user = user;
  
  free(pass);
  
  client_send(self->client, "auth\n\n");
  return 0;
}


static int method_search(self_t *self, char *p)
{
  char *search;
  library_query_t *query;
  track_t track;
  
  search = get_str(p, "query");
  if (!search) {
    client_error(self->client, "no_query");
    return 0;
  }
  
  query = library_query_new();
  if (!query) {
    musicd_log(LOG_ERROR, "protocol_musicd",
               "no query returned for search '%s'", search);
    client_error(self->client, "server_error");
    return -1;
  }
  
  library_query_filter(query, LIBRARY_FIELD_ALL, search);

  if (library_query_start(query)) {
    musicd_log(LOG_ERROR, "protocol_musicd", "can't start query");
    client_error(self->client, "server_error");
    return -1;
  }


  while (!library_query_next_track(query, &track)) {
    send_track(self->client, &track);
  }

  library_query_close(query);

  free(search);

  client_send(self->client, "search\n\n");
  return 0;
}

static int method_randomid(self_t *self, char *p)
{
  (void)p;
  int64_t id;
  id = library_randomid();
  
  client_send(self->client, "randomid\nid=%li\n\n", id);
  return 0;
}

static int method_open(self_t *self, char *p)
{
  track_t *track;
  int id;
  char *codec;
  int bitrate;
  transcoder_t *transcoder;
  stream_t *stream;
  
  id = get_int(p, "id");
  
  track = library_track_by_id(id);
  if (!track) {
    client_error(self->client, "track_not_found");
    return -1;
  }

  if (self->stream) {
    stream_close(self->stream);
  }
  
  self->stream = stream = stream_open(track);
  if (!self->stream) {
    client_error(self->client, "cannot_open");
    return -1;
  }
  
  codec = get_str(p, "codec");
  if (codec) {
    bitrate = get_int(p, "bitrate");
    /* No sense in re-encoding to same codec. */
    if (strcmp(codec, stream->format->codec)) {
      transcoder =
        transcoder_open(&stream->src_format, codec, bitrate);
      if (transcoder) {
        stream_set_transcoder(stream, transcoder);
      }
    }
  }
  
  send_track(self->client, track);
  
  client_send(self->client, "open\n");
  client_send(self->client, "codec=%s\n", stream->format->codec);
  client_send(self->client, "samplerate=%i\n", stream->format->samplerate);
  client_send(self->client, "bitspersample=%i\n",
              stream->format->bitspersample);
  client_send(self->client, "channels=%i\n", stream->format->channels);
  
  /* Replay gain */
  if (stream->replay_track_gain != 0.0) {
    client_send(self->client, "replaytrackgain=%f\n",
                stream->replay_track_gain);
  }
  if (stream->replay_album_gain != 0.0) {
    client_send(self->client, "replayalbumgain=%f\n",
                stream->replay_album_gain);
  }
  if (stream->replay_track_peak != 0.0) {
    client_send(self->client, "replaytrackpeak=%f\n",
                stream->replay_track_peak);
  }
  if (stream->replay_album_peak != 0.0) {
    client_send(self->client, "replayalbumpeak=%f\n",
                stream->replay_album_peak);
  }
  
  
  if (stream->format->extradata_size > 0) {
    client_send(self->client, "extradata:=%i\n\n",
                stream->format->extradata_size);
    
    client_write(self->client, (char *)stream->format->extradata,
                 stream->format->extradata_size);
  } else {
    client_send(self->client, "\n");
  }
  
  self->client->feed = true;
  
  return 0;
}

static int method_seek(self_t *self, char *p)
{
  int position;
  int result;
  
  if (!self->stream) {
    client_error(self->client, "nothing_open");
    return -1;
  }
  
  position = get_int(p, "position");
  
  result = stream_seek(self->stream, position);
    
  if (result < 0) {
    client_error(self->client, "cannot_seek");
    return -1;
  }

  client_send(self->client, "seek\n\n");
  
  /* client->feed is now false if the entire track was streamed */
  self->client->feed = true;
  return 0;
}

static int method_albumimg(self_t *self, char *p)
{
  int64_t album, size;
  int data_size;
  char *name, *cache_name;
  char *data;
  image_task_t *task;

  album = get_int(p, "album");
  size = get_int(p, "size");
  
  if (size < 16 || size > 512) {
    client_error(self->client, "invalid_size\n\n");
    return -1;
  }

  cache_name = image_cache_name(album, size);
  
  /* FIXME: Some better way to test this */
  name = library_album_image_path(album);
  if (!name) {
    client_send(self->client, "albumimg\nstatus=unavailable\n\n");
    goto exit;
  }
  free(name);
  
  if (!cache_exists(cache_name)) {
    task = malloc(sizeof(image_task_t));
    task->id = album;
    task->size = size;
    task_launch(image_album_task, (void *)task);
    client_send(self->client, "albumimg\nstatus=retry\n\n");
    goto exit;
  }
  
  data = cache_get(cache_name, &data_size);

  if (!data) {
    client_send(self->client, "albumimg\nstatus=unavailable\n\n");
    goto exit;
  }
  
  client_send(self->client, "albumimg\nimage:=%i\n\n", data_size);
  client_write(self->client, data, data_size);
  free(data);

exit:
  free(cache_name);
  return 0;
}

static int method_lyrics(self_t *self, char *p)
{
  char *lyrics;
  time_t ltime = 0;
  int64_t id = get_int(p, "track");
  
  lyrics = library_lyrics(id, &ltime);
  if (!lyrics) {
    if (ltime < (time(NULL) - 24 * 60 * 60)) {
      task_launch(lyrics_task, (void *)id);
      client_send(self->client, "lyrics\nstatus=retry\n\n");
    } else {
      client_send(self->client, "lyrics\nstatus=unavailable\n\n");
    }
  } else {
    client_send(self->client, "lyrics\nlyrics:=%d\n\n%s", strlen(lyrics),
                lyrics);
  }
  
  return 0;
}

struct method_entry {
  const char *name;
  int (*handler)(self_t *client, char *p);
  /*int flags;*/
};
static struct method_entry methods[] = {
  { "search", method_search },
  { "randomid", method_randomid },
  { "open", method_open },
  { "seek", method_seek },
  { "albumimg", method_albumimg },
  { "lyrics", method_lyrics },
  { NULL, NULL }
};

static int musicd_detect(const char *buf, size_t buf_size)
{  
  if (buf_size < strlen("musicd")) {
    return 0;
  }

  if (strbeginswith(buf, "musicd")) {
    return 1;
  }

  return -1;
}

static void *musicd_open(client_t *client)
{
  self_t *self = malloc(sizeof(self_t));
  memset(self, 0, sizeof(struct self));
  self->client = client;
  return self;
}

static void musicd_close(self_t *self)
{
  free(self->user);
  if (self->stream) {
    stream_close(self->stream);
  }

  free(self);
}

static int musicd_process(self_t *self, const char *buf, size_t buf_size)
{
  (void)buf_size;
  char *end, *method, *p;
  struct method_entry *entry;
  int result = 0;
  
  /* Do we have the entire packet? */
  end = strstr(buf, "\n\n");
  if (!end) {
    /* Not enough data */
    return 0;
  }
  end += 2;
  
  p = (char *) buf; /* Here goes our const-correctness... */

  method = line_read(&p);
  
  musicd_log(LOG_VERBOSE, "protocol_musicd", "method: '%s'", method);
  
  /* Special cases. */
  if (!strcmp(method, "musicd")) {
    result = method_musicd(self, p);
    goto exit;
  }
  if (!strcmp(method, "auth")) {
    result = method_auth(self, p);
    goto exit;
  }
  
  if (self->user == NULL) {
    client_error(self->client, "unauthorized");
    goto exit;
  }
  
  for (entry = methods; entry->name != NULL; ++entry) {
    if (!strcmp(entry->name, method)) {
      result = entry->handler(self, p);
      goto exit;
    }
  }
  
  client_error(self->client, "unknown_method");
  
exit:
  free(method);
  if (result < 0) {
    return result;
  }
  return end - buf;
}

static void musicd_feed(self_t *self)
{
  uint8_t *data;
  size_t size;
  int64_t pts;
  
  if (!self->stream) {
    /* What? */
    self->client->feed = false;
    return;
  }

  data = stream_next(self->stream, &size, &pts);
  
  if (!data) {
    client_send(self->client, "packet\npayload:=0\n\n");
    self->client->feed = false;
    return;
  }
  
  client_send(self->client, "packet\n");
  client_send(self->client, "pts=%li\n", pts);
  client_send(self->client, "payload:=%i\n", size);
  client_send(self->client, "\n");

  client_write(self->client, (const char *)data, size);
}


protocol_t protocol_musicd = {
  .name = "musicd",
  .detect = musicd_detect,
  .open = musicd_open,
  .close = (void(*)(void *)) musicd_close,
  .process = (int(*)(void *, const char *, size_t)) musicd_process,
  .feed = (void (*)(void *)) musicd_feed
};

