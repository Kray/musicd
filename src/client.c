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
#include "client.h"
#include "config.h"
#include "library.h"
#include "log.h"
#include "server.h"

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

client_t *client_new(int fd)
{
  client_t* result = malloc(sizeof(client_t));
  memset(result, 0, sizeof(client_t));
  
  result->fd = fd;
  
  client_send(result, "musicd\nprotocol=3\ncodecs=mp3\n\n");
  
  return result;
}

void client_close(client_t *client)
{
  if (client->stream) {
    stream_close(client->stream);
  }
  
  close(client->fd);
  free(client->user);
  free(client);
}

void client_send_track(client_t *client, track_t* track)
{
  client_send(client, "track\n");
  client_send(client, "id=%i\n", track->id);
  client_send(client, "path=%s\n", track->path);
  client_send(client, "track=%i\n", track->track);
  client_send(client, "title=%s\n", track->title);
  client_send(client, "artist=%s\n", track->artist);
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

static int get_int(char *src, const char *key)
{
  int result;
  char search[strlen(key) + 4];
  snprintf(search, strlen(key) + 4, "%s=%%i", key);
  
  for (; *src != '\n' && *src != '\0';) {
    if (sscanf(src, search, &result)) {
      return result;
    }
    for (; *src != '\n' && (*src + 1) != '\0'; ++src) { }
    ++src;
  }
  return 0;
}

static int client_error(client_t *client, const char *code)
{
  client_send(client, "error\nname=%s\n\n", code);
  return -1;
}


static int method_auth(client_t *client, char *p)
{
  char *user, *pass;
  
  user = get_str(p, "user");
  pass = get_str(p, "password");
  
  /*musicd_log(LOG_DEBUG, "client", "%s %s", user, pass);*/
  
  if (!user || strcmp(user, config_get("user"))
   || !pass || strcmp(pass, config_get("password"))) {
    free(user);
    free(pass);
    return client_error(client, "invalid_login");
  }
  
  client->user = user;
  
  free(pass);
  
  client_send(client, "auth\n\n");
  return 0;
}


static int method_search(client_t *client, char *p)
{
  char *search, *needle;
  library_query_t *query;
  track_t track;
  
  search = get_str(p, "query");
  
  /*if (!search) {
    client_error(client, "no_query");
    return 0;
  }*/
  
  needle = malloc(strlen(search) + 2 + 1);
  
  sprintf(needle, "%%%s%%", search);
  
  free(search);
  
  query = library_search(LIBRARY_TABLE_TRACKS, LIBRARY_FIELD_NONE, needle);
  if (!query) {
    musicd_log(LOG_ERROR, "client", "no query returned for search '%s'",
               needle);
  }
  
  while (!library_query_next(query, &track)) {
    client_send_track(client, &track);
  }
  
  library_query_close(query);
  
  free(needle);
  
  client_send(client, "search\n\n");
  return 0;
}

static int method_randomid(client_t *client, char *p)
{
  (void)p;
  int64_t id;
  id = library_randomid();
  
  client_send(client, "randomid\nid=%li\n\n", id);
  return 0;
}

static int method_open(client_t *client, char *p)
{
  track_t *track;
  int id;
  char *codec;
  int bitrate;
  transcoder_t *transcoder;
  
  id = get_int(p, "id");
  
  track = library_track_by_id(id);
  if (!track) {
    client_error(client, "track_not_found");
    return -1;
  }
  
  if (client->stream) {
    stream_close(client->stream);
  }
  
  client->stream = stream_open(track);
  if (!client->stream) {
    client_error(client, "cannot_open");
    return -1;
  }
  
  codec = get_str(p, "codec");
  if (codec) {
    bitrate = get_int(p, "bitrate");
    /* No sense in re-encoding to same codec. */
    if (strcmp(codec, client->stream->format->codec)) {
      transcoder =
        transcoder_open(&client->stream->src_format, codec, bitrate);
      if (transcoder) {
        stream_set_transcoder(client->stream, transcoder);
      }
    }
  }
  
  client_send_track(client, track);
  
  client_send(client, "open\n");
  client_send(client, "codec=%s\n", client->stream->format->codec);
  client_send(client, "samplerate=%i\n", client->stream->format->samplerate);
  client_send(client, "bitspersample=%i\n",
              client->stream->format->bitspersample);
  client_send(client, "channels=%i\n", client->stream->format->channels);
  
  /* Replay gain */
  if (client->stream->replay_track_gain != 0.0) {
    client_send(client, "replaytrackgain=%f\n", client->stream->replay_track_gain);  
  }
  if (client->stream->replay_album_gain != 0.0) {
    client_send(client, "replayalbumgain=%f\n", client->stream->replay_album_gain);
  }
  if (client->stream->replay_track_peak != 0.0) {
    client_send(client, "replaytrackpeak=%f\n", client->stream->replay_track_peak);
  }
  if (client->stream->replay_album_peak != 0.0) {
    client_send(client, "replayalbumpeak=%f\n", client->stream->replay_album_peak);
  }
  
  
  if (client->stream->format->extradata_size > 0) {
    client_send(client, "extradata:=%i\n\n", client->stream->format->extradata_size);
    
    write(client->fd, client->stream->format->extradata,
          client->stream->format->extradata_size);
  } else {
    client_send(client, "\n");
  }
  
  return 0;
}

static int method_seek(client_t *client, char *p)
{
  int position;
  int result;
  
  if (!client->stream) {
    client_error(client, "nothing_open");
    return -1;
  }
  
  position = get_int(p, "position");
  
  result = stream_seek(client->stream, position);
    
  if (result < 0) {
    client_error(client, "cannot_seek");
    return -1;
  }
  
  client_send(client, "seek\n\n");
  return 0;
}

struct method_entry {
  const char *name;
  int (*handler)(client_t *client, char *p);
  /*int flags;*/
};
static struct method_entry methods[] = {
  { "search", method_search },
  { "randomid", method_randomid },
  { "open", method_open },
  { "seek", method_seek },
  { NULL, NULL }
};

static int process_call(client_t *client)
{
  char *p = client->buf;
  char *method;
  int result;
  struct method_entry *entry;
  
  method = line_read(&p);
  
  musicd_log(LOG_VERBOSE, "client", "method: '%s'", method);
  
  /* auth is special case */
  if (!strcmp(method, "auth")) {
    result = method_auth(client, p);
    goto exit;
  }
  
  if (client->user == NULL) {
    client_error(client, "unauthorized");
    goto exit;
  }
  
  for (entry = methods; entry->name != NULL; ++entry) {
    if (!strcmp(entry->name, method)) {
      result = entry->handler(client, p);
      goto exit;
    }
  }
  
  client_error(client, "unknown_method");
  
exit:
  free(method);
  return result;
}

int client_process(client_t *client)
{
  char *tmp, buffer[1025];
  int n, result = 0;
  
  n = read(client->fd, buffer, 1024);
  if (n == 0) {
    musicd_log(LOG_INFO, "client", "client exits");
    return 1;
  }
  if (n < 0) {
    musicd_perror(LOG_INFO, "client", "terminating client");
    return 1;
  }
  
  if (client->buf_size == 0) {
    client->buf = malloc(n + 1);
  } else {
    tmp = malloc(client->buf_size + n + 1);
    memcpy(tmp, client->buf, client->buf_size);
    free(client->buf);
    client->buf = tmp;
  }
  
  memcpy(client->buf + client->buf_size, buffer, n);
  client->buf_size += n;
  
  if (client->buf[client->buf_size - 2] != '\n'
   || client->buf[client->buf_size - 1] != '\n') {
    /* Data not fed yet. */
    return 0;
  }
  
  client->buf[client->buf_size] = '\0';
  
  result = process_call(client);
  
  free(client->buf);
  client->buf = NULL;
  client->buf_size = 0;
  return result;
}


int client_send(client_t *client, const char *format, ...)
{
  int n, size = 128;
  char *buf;
  va_list va_args;

  buf = malloc(size);
  
  while (1) {
    va_start(va_args, format);
    n = vsnprintf(buf, size, format, va_args);
    va_end(va_args);
    
    if (n > -1 && n < size) {
      break;
    }
    
    if (n > -1) {
      size = n + 1;
    } else {
      size *= 2;
    }
    
    buf = realloc(buf, size);
  }
  n = write(client->fd, buf, n);
  free(buf);
  return n;
}


int client_next_packet(client_t* client)
{
  uint8_t *data;
  int size;
  int64_t pts;
  
  if (!client->stream) {
    /* what? */
    return -1;
  }
  
  data = stream_next(client->stream, &size, &pts);
  
  if (!data) {
    client_send(client, "packet\npayload:=0\n\n");
    return 0;
  }
  
  client_send(client, "packet\n");
  client_send(client, "pts=%li\n", pts);
  client_send(client, "payload:=%i\n", size);
  client_send(client, "\n");

  write(client->fd, data, size);
  
  return 0;
  
}


