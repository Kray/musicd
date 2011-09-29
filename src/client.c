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
  
  return result;
}

void client_close(client_t *client)
{
  if (client->track_stream) {
    track_stream_close(client->track_stream);
  }
  
  close(client->fd);
  free(client);
}

static char *str_read(char **string)
{
  char *result, *p;
  int len = 0;
  
  if (**string == '\n' || **string == '\0') {
    return NULL;
  }
  
  for (p = *string; ; ++p) {
    if (*p == ' ' || *p == '\n' || *p == '\0') {
      break;
    }
    if (*p == '\\') {
      ++len;
      ++p;
    }
    ++len;
  }

  result = calloc(len + 2, sizeof(char));
  len = 0;
  for (p = *string; ; ++p) {
    if (*p == '\\') {
      result[len++] = *(++p);
      continue;
    }
    
    if (*p == ' ') {
      ++p;
      break;
    }
    if (*p == '\n' || *p == '\0') {
      break;
    }
    result[len++] = *p;
  }
  result[len + 1] = '\0';
  *string = p;
  return result;
}

static char *escape_string(const char *src)
{
  if (!src) {
    return strdup("");
  }
  
  /* Enough for escaping all characters, as escape codes are two characters. */
  char tmp[strlen(src) * 2 + 1];
  int pos = 0;
  
  for (; *src != '\0';) {
    switch (*src) {
    case ' ':
      tmp[pos++] = '\\';
      tmp[pos] = ' ';
      break;
    case '\n':
      tmp[pos++] = '\\';
      tmp[pos] = 'n';
      break;
    case '\\':
      tmp[pos++] = '\\';
      tmp[pos] = '\\';
      break;
    default:
      tmp[pos] = *src;
    }
    ++src; ++pos;
  }
  tmp[pos] = '\0';
  return strdup(tmp);
}

static int str_read_int(char **string, int* result)
{
  char *str = str_read(string);
  int n = sscanf(str, "%i", result);
  free(str);
  return n != 1;
}

static int client_error(client_t *client, const char *code)
{
  char buffer[strlen(code) + 8];
  
  sprintf(buffer, "error %s\n", code);
  write(client->fd, buffer, strlen(buffer));
  return -1;
}

static int msg_hello(client_t *client, char *p)
{
  
  if (str_read_int(&p, &client->protocol)) {
    return client_error(client, "syntax");
  }
  
  client->protocol = 1;
  client->name = str_read(&p);
  
  musicd_log(LOG_INFO, "server", "Client '%s'.", client->name);
  
  client_send(client, "hello 1 musicd 0.0.1\n");
  return 0;
}

static int msg_auth(client_t *client, char *p)
{
  char *user, *pass;
  
  user = str_read(&p);
  pass = str_read(&p);
  
  if (strcmp(user, config_get("user"))
   || strcmp(pass, config_get("password"))) {
    free(user);
    free(pass);
    return client_error(client, "invalid_login");
  }
  
  client->user = user;
  
  free(pass);
  
  client_send(client, "auth admin\n");
  return 0;
}


static int msg_search(client_t *client, char *p)
{
  char line[1025];
  char *search, *needle;
  char *title, *artist, *album;
  
  search = str_read(&p);
  
  if (!search) {
    client_send(client, "error no_query\n");
    return 0;
  }
  
  needle = calloc(strlen(search) + 2 + 1, sizeof(char));
  
  sprintf(needle, "%%%s%%", search);
  
  free(search);
  
  library_query_t *query = library_search(needle);
  if (!query) {
    musicd_log(LOG_ERROR, "main", "No query.");
  }
  
  track_t track;
  while (!library_search_next(query, &track)) {
    title = escape_string(track.name);
    artist = escape_string(track.artist);
    album = escape_string(track.album);
    
    snprintf(line, 1024, "track %i number=%i title=%s artist=%s album=%s "
                          "duration=%i\n",
             track.id, track.number, title, artist, album, track.duration);
    client_send(client, line);
    
    /*musicd_log(LOG_DEBUG, "main", "%s", line);*/
    
    free(title);
    free(artist);
    free(album);
  }
  
  library_search_close(query);
  
  free(needle);
  
  client_send(client, "search\n");
  return 0;
}

static int msg_open(client_t *client, char *p)
{
  char line[1025];
  track_t *track;
  int id;
  char *title, *artist, *album;
  
  sscanf(p, "%d", &id);
  
  track = library_track_by_id(id);
  if (!track) {
    client_send(client, "error track_not_found\n");
    return -1;
  }
  
  if (client->track_stream) {
    track_stream_close(client->track_stream);
  }
  
  client->track_stream = track_stream_open(track);
  if (!client->track_stream) {
    client_send(client, "error cannot_open\n");
    return -1;
  }
  
  
  
  title = escape_string(client->track_stream->track->name);
  artist = escape_string(client->track_stream->track->artist);
  album = escape_string(client->track_stream->track->album);
  
  snprintf(line, 1024, "open %i number=%i title=%s artist=%s album=%s "
                       "duration=%i codec=%s samplerate=%i bitspersample=%i "
                        "channels=%i payload=%i\n",
           id, client->track_stream->track->number, title, artist, album,
           client->track_stream->track->duration,
           client->track_stream->codec,
           client->track_stream->samplerate,
           client->track_stream->bitspersample,
           client->track_stream->channels,
           client->track_stream->extradata_size);
  
  free(title);
  free(artist);
  free(album);
  
  /*musicd_log(LOG_DEBUG, "main", "%s", line);*/
  client_send(client, line);
  
  if (client->track_stream->extradata_size > 0) {
    write(client->fd, client->track_stream->extradata,
          client->track_stream->extradata_size);
  }
  
  return 0;
}

static int msg_seek(client_t *client, char *p)
{
  int position;
  int result;
  
  if (!client->track_stream) {
    client_send(client, "error nothing_open\n");
    return -1;
  }
  
  sscanf(p, "%i", &position);
  
  result = track_stream_seek(client->track_stream, position);
    
  if (result < 0) {
    client_send(client, "error cannot_seek\n");
    return -1;
  }
  
  client_send(client, "seek\n");
  return 0;
}

int client_process(client_t *client)
{
  char buffer[1024];
  int n, result;
  char *method;
  
  n = read(client->fd, buffer, 1023);
  if (n == 0) {
    musicd_log(LOG_INFO, "client", "Client exits.");
    return 1;
  }
  if (n < 0) {
    musicd_perror(LOG_INFO, "client", "Terminating client");
    return 1;
  }
  
  buffer[n] = '\0';

  char *p = buffer;
  method = str_read(&p);
  musicd_log(LOG_VERBOSE, "client", "Method: %s", method);
  
  if (!strcmp(method, "hello")) {
    result = msg_hello(client, p);
    goto exit;
  }
  
  if (client->protocol == 0) {
    client_error(client, "protocol_undefined");
    goto exit;
  }
  
  if (!strcmp(method, "auth")) {
    result = msg_auth(client, p);
    goto exit;
  }
  
  if (client->user == NULL) {
    client_error(client, "unauthorized");
    goto exit;
  }
  
  if (!strcmp(method, "search")) {
    result = msg_search(client, p);
    goto exit;
  }
  
  if (!strcmp(method, "open")) {
    result = msg_open(client, p);
    goto exit;
  }
  
  if (!strcmp(method, "seek")) {
    result = msg_seek(client, p);
    goto exit;
  }
  
  client_send(client, "error unknown_method\n");
  
  
exit:
  free(method);
  return result;
}


int client_send(client_t *client, const char *msg)
{
  return write(client->fd, msg, strlen(msg));
}


int client_next_packet(client_t* client)
{
  uint8_t *data;
  int size;
  int64_t pts;
  char line[1024];
  
  if (!client->track_stream) {
    /*client_send(client, "error no_track_open\n");*/
    /* what? */
    return -1;
  }
  
  data = track_stream_read(client->track_stream, &size, &pts);
  
  if (!data) {
    client_send(client, "packet payload=0\n");
    return 0;
  }
  
  snprintf(line, 1023, "packet pts=%li payload=%i\n", pts, size);
  client_send(client, line);
  
  /*musicd_log(LOG_DEBUG, "main", "%s", line);*/
  
  write(client->fd, data, size);
  
  return 0;
  
}


