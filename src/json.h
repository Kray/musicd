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
#ifndef MUSICD_JSON_H
#define MUSICD_JSON_H

#include "strings.h"

#include <stdint.h>

/**
 * Trivial JSON serializer
 */
typedef struct json {
  string_t *buf;
  int comma;
} json_t;

void json_init(json_t *json);
void json_finish(json_t *json);
const char *json_result(json_t *json);

void json_object_begin(json_t *json);
void json_object_end(json_t *json);
void json_array_begin(json_t *json);
void json_array_end(json_t *json);
void json_define(json_t *json, const char *name);
void json_int(json_t *json, int i);
void json_int64(json_t *json, int64_t i);
void json_string(json_t *json, const char *string);

#endif
