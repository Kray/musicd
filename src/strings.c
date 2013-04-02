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
#include "strings.h"

#include "log.h"

#include <assert.h>
#include <ctype.h>
#include <iconv.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

string_t *string_new()
{
  string_t *string = malloc(sizeof(string_t));
  string->string = malloc(64 + 1);
  string->string[0] = '\0';
  string->size = 0;
  string->max_size = 64;
  return string;
}

string_t *string_from(const char *string2)
{
  string_t *string = string_new();
  string->string = strdup(string2);
  string->size = strlen(string->string);
  string->max_size = string->size;
  return string;
}

char *string_release(string_t *string)
{
  char *result = string->string;
  free(string);
  return result;
}

void string_free(string_t *string)
{
  free(string->string);
  free(string);
}

void string_ensure_space(string_t *string, size_t size)
{
  if (string->max_size >= size) {
    return;
  }
  
  /* Minimum growth is always double size to reduce unnecessary reallocs. */
  while (string->max_size < size) {
    string->max_size *= 2;
  }
  
  string->string = realloc(string->string, string->max_size + 1);
}

const char *string_string(string_t *string)
{
  return string->string;
}

size_t string_size(string_t *string)
{
  return string->size;
}

void string_append(string_t *string, const char *string2)
{
  string_nappend(string, string2, strlen(string2));
}

void string_appendf(string_t *string, const char *format, ...)
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
  string_nappend(string, buf, n);
  free(buf);
}

void string_nappend(string_t *string, const char *string2, size_t addlen)
{
  size_t newlen;

  newlen = string->size + addlen;
  
  string_ensure_space(string, newlen);
  
  memcpy(string->string + string->size, string2, addlen);
  
  string->size = newlen;

  string->string[string->size] = '\0';
}

void string_push_back(string_t *string, char c)
{
  string_ensure_space(string, string->size + 1);
  string->string[string->size++] = c;
  string->string[string->size] = '\0';
}

void string_remove_front(string_t *string, size_t n)
{
  assert(string->size >= n);

  string->size -= n;
  memmove(string->string, string->string + n, string->size);
  string->string[string->size] = '\0';
}

string_t *string_iconv(string_t *string, const char *to, const char *from)
{
  int n;
  char *inbuf = string->string, *outbuf;
  char buf[4096 + 1];
  
  size_t inleft = string->size, outleft;
  
  string_t *result = string_new();
  
  iconv_t conv = iconv_open(to, from);
  
  while (inleft > 0) {
    outbuf = buf;
    outleft = 4096;
    
    n = iconv(conv, &inbuf, &inleft, &outbuf, &outleft);
    if (n == -1) {
      musicd_perror(LOG_DEBUG, "strings", "iconv failed");
      return result;
    }
    
    *outbuf = '\0';
    string_append(result, buf);
  }
  
  iconv_close(conv);
  return result;
}

char *stringf(const char *format, ...)
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
  return buf;
}

const char *strcasestr(const char *haystack, const char *needle)
{
  const char *p;
  int pos;
  
  for (p = haystack; *p != '\0'; ++p) {
    for (pos = 0; needle[pos] != '\0'; ++pos) {
      if (tolower(p[pos]) != tolower(needle[pos])) {
        goto next;
      }
    }
    return p;
  next:
    continue;
  }
  return NULL;
}

const char *strchrnull(const char *s, int c)
{
  const char *result = strchr(s, c);
  if (!result) {
    result = s + strlen(s);
  }
  return result;
}

const char *strstrnull(const char *s, const char *c)
{
  const char *result = strstr(s, c);
  if (!result) {
    result = s + strlen(s);
  }
  return result;
}

int strbeginswith(const char *string, const char *what)
{
  size_t string_len = strlen(string), what_len = strlen(what);

  if (string_len < what_len) {
    return 0;
  }

  return !strncmp(string, what, what_len);
}


char *strextract(const char *begin, const char *end)
{
  size_t size;
  char *result;

  if (!begin) {
    return NULL;
  }

  if (!end) {
    /* Find terminating NULL */
    for (end = begin; *end != '\0'; ++end) { }
  }

  assert(begin <= end);

  size = end - begin;

  result = malloc(size + 1);
  memcpy(result, begin, size);
  result[size] = '\0';
  return result;
}
