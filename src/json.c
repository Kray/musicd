/*
 * This file is part of musicd.
 * Copyright (C) 2012 Konsta Kokkinen <kray@tsundere.fi>
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
#include "json.h"

#include <inttypes.h>

static void comma(json_t *json)
{
  if (json->comma) {
    string_append(json->buf, ",");
    json->comma = 0;
  }
}

void json_init(json_t *json)
{
  json->buf = string_new();
  json->comma = 0;
}

void json_finish(json_t *json)
{
  string_free(json->buf);
}

const char *json_result(json_t *json)
{
  return string_string(json->buf);
}

void json_object_begin(json_t *json)
{
  comma(json);
  string_append(json->buf, "{");
}

void json_object_end(json_t *json)
{
  string_append(json->buf, "}");
  json->comma = 1;
}

void json_array_begin(json_t *json)
{
  comma(json);
  string_append(json->buf, "[");
}

void json_array_end(json_t *json)
{
  string_append(json->buf, "]");
  json->comma = 1;
}

void json_define(json_t *json, const char *name)
{
  comma(json);
  string_appendf(json->buf, "\"%s\":", name);
}

void json_int(json_t *json, int i)
{
  comma(json);
  string_appendf(json->buf, "%d", i);
  json->comma = 1;
}

void json_int64(json_t *json, int64_t i)
{
  comma(json);
  string_appendf(json->buf, "%" PRId64 "", i);
  json->comma = 1;
}

static const char *escape_from = "\"\\\b\f\n\r\t";
static const char *escape_to = "\"\\bfnrt";
static const int n_escape = 7;

void json_string(json_t *json, const char *string)
{
  int i;

  comma(json);

  string_push_back(json->buf, '"');

  if (string) {
    for (; *string != '\0'; ++string) {
      for (i = 0; i < n_escape; ++i) {
        if (*string == escape_from[i]) {
          break;
        }
      }
      if (i < n_escape) {
        string_push_back(json->buf, '\\');
        string_push_back(json->buf, escape_to[i]);
      } else {
        string_push_back(json->buf, *string);
      }
    }
  }

  string_push_back(json->buf, '"');
  json->comma = 1;
}
