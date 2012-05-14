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
#include "url.h"

#include "log.h"
#include "strings.h"

#include <stdlib.h>
#include <string.h>

#include <curl/curl.h>

struct curl_buffer {
  char *data;
  size_t size;
  size_t max_size;
};

static size_t
write_memory_function(void *data, size_t size, size_t nmemb, void *opaque)
{
  size_t realsize = size * nmemb;
  struct curl_buffer *buffer = (struct curl_buffer *)opaque;
  
  if (buffer->size + realsize > buffer->max_size) {
    while (buffer->size + realsize > buffer->max_size) {
      buffer->max_size *= 2;
    }
    buffer->data = realloc(buffer->data, buffer->max_size + 1);
  }
  
  memcpy(buffer->data + buffer->size, data, realsize);
  buffer->size += realsize;
  buffer->data[buffer->size] = '\0';
  
  return realsize;
}

char *url_fetch(const char *url)
{
  struct curl_buffer data;
  CURL *curl;
  char errorbuf[CURL_ERROR_SIZE];
  CURLcode result;
  
  data.data = malloc(1025);
  data.max_size = 1024;
  data.size = 0;

  curl = curl_easy_init();
  
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_memory_function);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
  
  result = curl_easy_perform(curl);
  if (result) {
    musicd_log(LOG_ERROR, "url", "fetching '%s' failed: %s", url, errorbuf);
    free(data.data);
    data.data = NULL;
  }
  
  curl_easy_cleanup(curl);
  return data.data;
}


char* url_fetch_escaped_location(const char* server, const char* location)
{
  char *encoded, *url, *result;
  
  encoded = curl_escape(location, strlen(location));
  url = stringf("%s/%s", server, encoded);
  curl_free(encoded);
  
  result = url_fetch(url);
  free(url);
  return result;
}

