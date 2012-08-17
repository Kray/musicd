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
#ifndef MUSICD_STRINGS_H
#define MUSICD_STRINGS_H

#include <string.h>

/**
 * General container and routines for string operations. Implementation doesn't
 * care what kind of data is actually stored, but ensures the string is always
 * null terminated.
 */
typedef struct string {
  char *string;
  size_t size;
  size_t max_size;
} string_t;

string_t *string_new();

string_t *string_from(const char *string2);

/** Frees @p string and returns internal buffer which must be freed later. */
char *string_release(string_t *string);

void string_free(string_t *string);

/** Ensures @p string has at least @p size bytes of space. */
void string_ensure_space(string_t *string, size_t size);

const char *string_string(string_t *string);
size_t string_size(string_t *string);

void string_append(string_t *string, const char *string2);
void string_nappend(string_t *string, const char *string2, size_t n);

void string_push_back(string_t *string, char c);

void string_remove_front(string_t *string, size_t n);

string_t *string_iconv(string_t *string, const char *to, const char *from);

/** Like printf, but returns string instead of outputting it. Return value
 * must be freed later. */
char *stringf(const char *format, ...);

/** Case-insensitive strstr. */
const char *strcasestr(const char *haystack, const char *needle);

#endif
