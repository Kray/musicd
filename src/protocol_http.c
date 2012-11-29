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
#include "protocol_http.h"

#include "cache.h"
#include "client.h"
#include "config.h"
#include "image.h"
#include "json.h"
#include "log.h"
#include "musicd.h"
#include "query.h"
#include "strings.h"
#include "task.h"

#include "library.h"

typedef struct http {
  client_t *client;

  /* Current request */
  const char *request;
  char *query;
  char *path;
  char *args;

} http_t;

static const char *args_ptr(http_t *http, const char *key)
{
  const char *p = http->args;

  if (!http->args) {
    return NULL;
  }

  while (1) {
    if (strbeginswith(p, key)) {
      return p + strlen(key);
    }
    
    p = strchr(p, '&');
    if (!p) {
      break;
    }
    ++p;
  }
  return NULL;
}

static int64_t args_int(http_t *http, const char *key)
{
  const char *p = args_ptr(http, key);
  int64_t result = 0;

  if (!p) {
    return 0;
  }

  /* The parameter is set but it has no value */
  if (*p != '=') {
    return 0;
  }
  
  sscanf(p, "=%" PRId64 "", &result);
  
  return result;
}

static bool args_bool(http_t *http, const char *key)
{
  const char *p = args_ptr(http, key);
  return p ? true : false;
}

static char *args_str(http_t *http, const char *key)
{
  const char *p = args_ptr(http, key);
  string_t *result;
  char tmp;

  if (!p) {
    return NULL;
  }
  
  /* The parameter is set but it has no value */
  if (*p != '=') {
    return strdup("");
  }
  
  result = string_new();
  for (++p; *p != '&' && *p != '\0'; ++p) {
    if (*p != '%') {
      if (*p == '+') { /* + means space */
        string_push_back(result, ' ');
      } else {
        string_push_back(result, *p);
      }
      continue;
    }
    
    ++p;

    if (!isxdigit(*p) || !isxdigit(*(p+1)) || sscanf(p, "%2hhx", &tmp) != 1) {
      string_free(result);
      return NULL;
    }
    string_push_back(result, tmp);
    
    ++p;
  }
  
  return string_release(result);
}

/**
 * Begins HTTP headers
 * @param status default 200 OK if NULL
 */
static void http_begin_headers
  (http_t *http,
   const char *status,
   const char *content_type,
   int64_t content_length)
{
  client_send(http->client, "HTTP/1.1 %s\r\n", status ? status : "200 OK");
  client_send(http->client, "Server: musicd/" MUSICD_VERSION_STRING "\r\n");
  if (content_length >= 0) {
    client_send(http->client, "Content-Length: %d\r\n", content_length);
  }
  if (content_type) {
    client_send(http->client, "Content-Type: %s; charset=utf-8\r\n",
                content_type);
  }
  /* Cross-site scripting - allows accessing the server with browser from
   * different origin, but can be a security hole. */
  if (config_to_bool("enable-xss")) {
    client_send(http->client, "Access-Control-Allow-Origin: *\r\n");
    client_send(http->client, "Access-Control-Allow-Credentials: *\r\n");
  }
}

static void http_send_headers
  (http_t *http,
   const char *status,
   const char *content_type,
   int64_t content_length)
{
  http_begin_headers(http, status, content_type, content_length);
  client_send(http->client, "\r\n");
}

/**
 * @param status default 200 OK if NULL
 * @param content_type default text/html if NULL
 */
static void http_send
  (http_t *http,
   const char *status,
   const char *content_type,
   size_t content_length,
   const char *content)
{
  http_send_headers(http,
              status,
              content_type ? content_type : "text/html",
              content_length);
  client_write(http->client, content, content_length);
}

static void http_send_text
  (http_t *http,
   const char *status,
   const char *content_type,
   const char *content)
{
  http_send(http, status, content_type, strlen(content), content);
}

static void http_reply(http_t *http, const char *status)
{
  http_send(http, status, "text/plain", strlen(status), status);
}

static void http_send_file
  (http_t *http, const char *path, const char *content_type)
{
  size_t size;
  char *data;
  FILE *file = fopen(path, "rb");
  if (!file) {
    http_reply(http, "404 Not Found");
    return;
  }

  fseek(file, 0, SEEK_END);
  size = ftell(file);
  fseek(file, 0, SEEK_SET);

  if (size <= 0) {
    http_reply(http, "404 Not Found");
    fclose(file);
    return;
  }

  data = malloc(size);
  size = fread(data, 1, size, file);
  fclose(file);

  http_send(http, NULL, content_type, size, data);

  free(data);
}

static char *decode_url(const char **p)
{
  char tmp;
  string_t *result = string_new();

  for (; **p != '&' && **p != '\0'; ++*p) {
    if (**p != '%') {
      if (**p == '+') { /* + means space */
        string_push_back(result, ' ');
      } else {
        string_push_back(result, **p);
      }
      continue;
    }    
    ++*p;

    if (!isxdigit(**p) || !isxdigit(*(*p+1)) || sscanf(*p, "%2hhx", &tmp) != 1) {
      string_free(result);
      return NULL;
    }
    string_push_back(result, tmp);
    
    ++*p;
  }
  return string_release(result);
}

static char *encode_url(const char *p)
{
  string_t *result = string_new();

  for (; *p != '\0'; ++p) {
    if ((*p >= 'A' && *p <= 'Z')
     || (*p >= 'a' && *p <= 'z')
     || (*p >= '0' && *p <= '9')) {
      string_push_back(result, *p);
      continue;
    }
    string_appendf(result, "%%%2hhx", *p);
  }
  return string_release(result);
}


/** Iterates through all arguments and sets all valid filters to query. */
static int parse_query_filters(http_t *http, query_t *query)
{
  const char *args = http->args, *p;
  char *name, *value;
  query_field_t field;

  if (!args) {
    return 0;
  }
  p = args;
  while (1) {
    for (args = p; *p != '\0' && *p != '=' && *p != '&'; ++p) { }

    if (*p == '\0') {
      break;
    }
    if (*p == '&') {
      args = ++p;
      continue;
    }

    name = strextract(args, p);
    field = query_field_from_string(name);
    free(name);
    
    ++p;

    if (!field) {
      for (; *p != '\0' && *p != '&'; ++p) { }
    } else {
      value = decode_url(&p);
      if (value) {
        query_filter(query, field, value);
        free(value);
      }
    }

    if (*p == '\0') {
      break;
    }
    ++p;
  }
  return 0;
}

static void parse_query_bounds(http_t *http, query_t *query)
{
  int64_t limit = args_int(http, "limit"),
          offset = args_int(http, "offset");
  if (limit > 0) {
    query_limit(query, limit);
  }
  if (offset > 0) {
    query_offset(query, offset);
  }
}

static void parse_query_sort(http_t *http, query_t *query)
{
  char *sort = args_str(http, "sort");
  if (!sort) {
    return;
  }
  query_sort_from_string(query, sort);
  free(sort);
}

static int64_t parse_total(http_t *http, query_t *query)
{
  if (!args_bool(http, "total")) {
    return 0;
  }
  return query_count(query);
}

static int method_musicd(http_t *http)
{
  json_t json;

  json_init(&json);
  json_object_begin(&json);
  json_define(&json, "name"); json_string(&json, config_get("server-name"));
  json_define(&json, "version");  json_string(&json, MUSICD_VERSION_STRING);
  json_define(&json, "http-api"); json_string(&json, "1");

  json_define(&json, "codecs");
  json_array_begin(&json);
    json_string(&json, "mp3");
  json_array_end(&json);
  json_define(&json, "bitrate-min"); json_int(&json, 64000);
  json_define(&json, "bitrate-max"); json_int(&json, 320000);

  json_define(&json, "image-sizes");
  json_array_begin(&json);
    json_int(&json, 16);
    json_int(&json, 32);
    json_int(&json, 64);
    json_int(&json, 128);
    json_int(&json, 256);
    json_int(&json, 512);
  json_array_end(&json);

  json_object_end(&json);

  http_send_text(http, "200 OK", "text/json", json_result(&json));

  json_finish(&json);
  return 0;
}

static int method_auth(http_t *http)
{
  static const char *response_ok = "{\"auth\":\"ok\"}";
  static const char *response_error = "{\"auth\":\"error\"}";

  char *user, *password;
  char *set_user = NULL, *set_password = NULL;

  user = args_str(http, "user");
  password = args_str(http, "password");

  if (!user || !password) {
    http_reply(http, "400 Bad Requst");
    goto finish;
  }

  if (strcmp(user, config_get("user"))
   || strcmp(password, config_get("password"))) {

    musicd_log(LOG_VERBOSE, "protocol_http", "%s failed auth",
               http->client->address);
    http_send_text(http, "200 OK", "text/json", response_error);

  } else {
    set_user = encode_url(config_get("user"));
    set_password = encode_url(config_get("password"));
    musicd_log(LOG_VERBOSE, "protocol_http", "%s authed",
               http->client->address);

    http_begin_headers(http, "200 OK", "text/json", strlen(response_ok));
    client_send(http->client,
                "Set-Cookie: user=%s;\r\n"
                "Set-Cookie: password=%s;\r\n"
                "\r\n%s",
                set_user, set_password, response_ok);
  }

finish:
  free(user);
  free(password);
  free(set_user);
  free(set_password);
  return 0;
}

static int method_tracks(http_t *http)
{
  query_t *query = query_tracks_new();
  int64_t total;
  json_t json;
  track_t track;

  parse_query_filters(http, query);

  total = parse_total(http, query);
  if (total < 0) {
    musicd_log(LOG_ERROR, "protocol_http", "query_count failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  parse_query_bounds(http, query);
  parse_query_sort(http, query);
  
  if(query_start(query)) {
    musicd_log(LOG_ERROR, "protocol_http", "query_start failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  json_init(&json);
  json_object_begin(&json);
  
  if (total) {
    json_define(&json, "total");
    json_int64(&json, total);
  }
  
  json_define(&json, "tracks");
  json_array_begin(&json);
  
  while (!query_tracks_next(query, &track)) {
    json_object_begin(&json);
    json_define(&json, "id");       json_int64(&json, track.id);
    json_define(&json, "track");    json_int(&json, track.track);
    json_define(&json, "title");    json_string(&json, track.title);
    json_define(&json, "artistid"); json_int64(&json, track.artistid);
    json_define(&json, "artist");   json_string(&json, track.artist);
    json_define(&json, "albumid");  json_int64(&json, track.albumid);
    json_define(&json, "album");    json_string(&json, track.album);
    json_define(&json, "duration"); json_int(&json, track.duration);
    json_object_end(&json);
  }
  json_array_end(&json);
  json_object_end(&json);
  
  http_send_text(http, "200 OK", "text/json", json_result(&json));

  json_finish(&json);

finish:
  query_close(query);
  return 0;
}

static int method_artists(http_t *http)
{
  query_t *query = query_artists_new();
  int64_t total;
  json_t json;
  query_artist_t artist;

  parse_query_filters(http, query);

  total = parse_total(http, query);
  if (total < 0) {
    musicd_log(LOG_ERROR, "protocol_http", "query_count failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  parse_query_bounds(http, query);
  parse_query_sort(http, query);
  
  if(query_start(query)) {
    musicd_log(LOG_ERROR, "protocol_http", "query_start failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  json_init(&json);
  json_object_begin(&json);
  
  if (total) {
    json_define(&json, "total");
    json_int64(&json, total);
  }
  
  json_define(&json, "artists");
  json_array_begin(&json);
  
  while (!query_artists_next(query, &artist)) {
    json_object_begin(&json);
    json_define(&json, "id");       json_int64(&json, artist.artistid);
    json_define(&json, "artist");    json_string(&json, artist.artist);
    json_object_end(&json);
  }
  json_array_end(&json);
  json_object_end(&json);
  
  http_send_text(http, "200 OK", "text/json", json_result(&json));
  
  json_finish(&json);

finish:
  query_close(query);
  return 0;
}

static int method_albums(http_t *http)
{
  query_t *query = query_albums_new();
  int64_t total;
  json_t json;
  query_album_t album;

  parse_query_filters(http, query);

  total = parse_total(http, query);
  if (total < 0) {
    musicd_log(LOG_ERROR, "protocol_http", "query_count failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  parse_query_bounds(http, query);
  parse_query_sort(http, query);
  
  if(query_start(query)) {
    musicd_log(LOG_ERROR, "protocol_http", "query_start failed");
    http_reply(http, "500 Internal Server Error");
    goto finish;
  }
  
  json_init(&json);
  json_object_begin(&json);
  
  if (total) {
    json_define(&json, "total");
    json_int64(&json, total);
  }
  
  json_define(&json, "albums");
  json_array_begin(&json);
  
  while (!query_albums_next(query, &album)) {
    json_object_begin(&json);
    json_define(&json, "id");       json_int64(&json, album.albumid);
    json_define(&json, "album");    json_string(&json, album.album);
    json_object_end(&json);
  }
  json_array_end(&json);
  json_object_end(&json);
  
  http_send_text(http, "200 OK", "text/json", json_result(&json));

  json_finish(&json);

finish:
  query_close(query);
  return 0;
}

static int64_t validate_image_size(int64_t size)
{
  if (size == 0) {
    return 0;
  }
  if (size < 16) {
    return 16;
  }
  if (size > 512) {
    return 512;
  }
  return size;
}

static int send_image(http_t *http, char *cache_name)
{
  char *data;
  int data_size;
  data = cache_get(cache_name, &data_size);
  if (!data) {
    http_reply(http, "404 Not Found");
  } else {
    http_send(http, "200 OK", "image/jpeg", data_size, data);
  }

  free(data);
  free(cache_name);
  return 0;
}

static int method_image(http_t *http)
{
  int64_t image, size;
  char *cache_name, *path;
  task_t *task;
  image_task_t *task_args;

  image = args_int(http, "id");
  size = args_int(http, "size");

  if (image <= 0) {
    http_reply(http, "400 Bad Request");
    return 0;
  }

  size = validate_image_size(size);
  if (!size) {
    path = library_image_path(image);
    if (!path) {
      http_reply(http, "404 Not Found");
      return 0;
    }
    http_send_file(http, path, image_mime_type(path));
    free(path);
    return 0;
  }

  cache_name = image_cache_name(image, size);
  if (cache_exists(cache_name)) {
    return send_image(http, cache_name);
  }

  task_args = malloc(sizeof(image_task_t));
  task_args->id = image;
  task_args->size = size;
  task = task_start(image_task, (void *)task_args);
  client_wait_task(http->client, task, (client_callback_t)send_image, cache_name);
  return 0;
}

static int method_album_image(http_t *http)
{
  int64_t album, size, image;

  album = args_int(http, "id");
  size = args_int(http, "size");

  image = library_album_image(album);
  if (image <= 0) {
    http_reply(http, "404 Not Found");
    return 0;
  }

  client_send(http->client,
              "HTTP/1.1 302 Found\r\n"
              "Server: musicd/" MUSICD_VERSION_STRING "\r\n"
              "Location: /image?id=%" PRId64 "&size=%" PRId64 "\r\n"
              "\r\n", image, size);
  return 0;
}

static bool album_images_cb(library_image_t *image, json_t *json)
{
  json_int64(json, image->id);
  return true;
}

static int method_album_images(http_t *http)
{
  int64_t album;
  json_t json;

  album = args_int(http, "id");
  if (album <= 0) {
    http_reply(http, "400 Bad Request");
    return 0;
  }

  json_init(&json);
  json_object_begin(&json);
  json_define(&json, "images");
  json_array_begin(&json);
  library_iterate_images_by_album(album,
    (bool(*)(library_image_t *, void *))album_images_cb, &json);
  json_array_end(&json);
  json_object_end(&json);
  http_send_text(http, "200 OK", "text/json", json_result(&json));
  json_finish(&json);

  return 0;
}

/* Allow access without authorization */
#define NO_AUTH 0x02

struct method_entry {
  const char *name;
  int (*handler)(http_t *http);
  int flags;
};
static struct method_entry methods[] = {
  { "/musicd", method_musicd, NO_AUTH },
  { "/auth", method_auth, NO_AUTH },

  { "/tracks", method_tracks, 0 },
  { "/artists", method_artists, 0 },
  { "/albums", method_albums, 0 },

  { "/image", method_image, 0 },
  { "/album/image", method_album_image, 0 },
  { "/album/images", method_album_images, 0 },

  { NULL, NULL, 0 }
};

struct mime_entry {
  const char *extension;
  const char *mime;
};
static struct mime_entry mime_types[] = {
  { "html", "text/html" },
  { "css", "text/css" },
  { "js", "text/x-javascript" },
  { "jpg", "image/jpeg" },
  { "png", "image/png" },
  { NULL, NULL },
};

/* Tests if the user is authorized */
static int is_authorized(http_t *http)
{
  int result;
  char *encoded, *search;

  /* TODO: real cookie parsing... */
  encoded = encode_url(config_get("user"));
  search = stringf("user=%s", encoded);
  if (!strstr(http->request, search)) {
    result = 0;
    goto finish;
  }

  encoded = encode_url(config_get("password"));
  search = stringf("password=%s", encoded);
  if (!strstr(http->request, search)) {
    result = 0;
    goto finish;
  }

  result = 1;

finish:
  free(encoded);
  free(search);
  return result;
}

static int process_request(http_t *http)
{
  struct method_entry *method;
  char *path, *ext;
  struct mime_entry *mime;

  for (method = methods; method->name != NULL; ++method) {
    if (!strcmp(method->name, http->path)) {
      if (!(method->flags & NO_AUTH)) {
        if (!is_authorized(http)) {
          http_reply(http, "403 Forbidden");
          return 0;
        }
      }
      return method->handler(http);
    }
  }

  /* Not any method, try HTTP root */
  if (!config_get_value("http-root")) {
    http_reply(http, "404 Not Found");
    return 0;
  }

  if (!strcmp(http->path, "/")) {
    path = stringf("%s/index.html", config_to_path("http-root"));
    http_send_file(http, path, "text/html");
    free(path);
    return 0;
  }

  path = stringf("%s/%s", config_to_path("http-root"), http->path);
  if (strstr(path, "/../")) {
    /* Let's just assume someone is doing something bad */
    http_reply(http, "403 Forbidden");
    free(path);
    return 0;
  }

  musicd_log(LOG_DEBUG, "protocol_http", "static path: %s", path);

  for (ext = path + strlen(path); *(ext - 1) != '.' && *(ext - 1) != '/';
       --ext) { }

  for (mime = mime_types; mime->extension != NULL; ++mime) {
    if (!strcmp(mime->extension, ext)) {
      break;
    }
  }

  http_send_file(http, path,
                 mime->mime ? mime->mime : "application/octet-stream");
  free(path);
  return 0;
}

static int http_detect(const char *buf, size_t buf_size)
{
  (void)buf_size;
  
  if (!config_to_bool("enable-http")) {
    return -1;
  }

  if (strbeginswith(buf, "GET ")
   || strbeginswith(buf, "HEAD ")) {
    return 1;
  }
  
  if (buf_size < strlen("HEAD ")) {
    return 0;
  }

  return -1;
}

static void *http_open(client_t *client)
{
  http_t *http = malloc(sizeof(http_t));
  memset(http, 0, sizeof(http_t));
  http->client = client;
  return http;
}

static void http_close(void *self)
{
  http_t *http = (http_t *)self;
  free(http);
}

static int http_process(void *self, const char *buf, size_t buf_size)
{
  (void)buf_size;
  http_t *http = (http_t *)self;
  const char *end, *p1, *p2;
  int result = 0;

  /* Do we have all headers? */
  end = strstr(buf, "\r\n\r\n");
  if (!end) {
    /* Not enough data */
    return 0;
  }
  end += 4;

  /* Is this an HTTP method we can handle? */
  if (strbeginswith(buf, "GET ")
   || strbeginswith(buf, "HEAD ")) {
  } else {
    musicd_log(LOG_VERBOSE, "protocol_http",
               "unsupported http method (not GET or HEAD)");
    http_reply(http, "400 Bad Request");
    return -1;
  }
  http->request = buf;
  
  /* Extract HTTP query */
  for (p1 = buf; *p1 != ' '; ++p1) { }
  ++p1;
  if (*p1 != '/') {
    /* Not valid */
    http_reply(http, "400 Bad Request");
    return -1;
  }

  for (p2 = p1; *p2 != ' '; ++p2) {
    if (*p2 == '\0' || *p2 == '\r' || *p2 == '\n') {
      musicd_log(LOG_VERBOSE, "protocol_http",
                 "malformed request line (no tailing version)");
      musicd_log(LOG_DEBUG, "protocol_http", "request was:\n%s", buf);
      http_reply(http, "400 Bad Request");
      return -1;
    }
  }

  http->query = strextract(p1, p2);
  
  musicd_log(LOG_VERBOSE, "protocol_http", "query: %s", http->query);

  /* Extract path and arguments from query */
  p2 = strchr(http->query, '?');
  if (!p2) {
    /* No arguments */
    http->path = strdup(http->query);
    http->args = NULL;
  } else {
    http->path = strextract(http->query, p2);
    http->args = strextract(p2 + 1, NULL);
  }
  
  result = process_request(http);

  free(http->query);
  free(http->path);
  free(http->args);

  if (result < 0) {
    return result;
  }
  return end - buf;
}

protocol_t protocol_http = {
  .name = "http",
  .detect = http_detect,
  .open = http_open,
  .close = http_close,
  .process = http_process,
  /*.feed = */
};

