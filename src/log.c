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
#include "log.h"

#include "libav.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char *log_prefix[] = 
  { "FATAL", "ERROR", "Warning", "Info", "Verbose", "Debug" };

int log_level = LOG_INFO;

void musicd_log(int level, const char* subsys, const char* fmt, ...)
{
  va_list va_args;
  if (level > log_level) {
    return;
  }
  printf("[%s]:%s: ",log_prefix[level], subsys);
  va_start(va_args, fmt);
  vprintf(fmt, va_args);
  va_end(va_args);
  printf("\n");
}

void musicd_perror(int level, const char* subsys, const char* fmt, ... )
{
  va_list va_args;
  if (level > log_level) {
    return;
  }
  printf("[%s]:%s: ",log_prefix[level], subsys);
  va_start(va_args, fmt);
  vprintf(fmt, va_args);
  va_end(va_args);
  printf(": %s\n", strerror(errno));
}

void log_level_changed(char *level)
{
  if (!strcmp(level, "fatal")) {
    log_level = LOG_FATAL;
  }
  if (!strcmp(level, "error")) {
    log_level = LOG_ERROR;
  }
  if (!strcmp(level, "warning")) {
    log_level = LOG_WARNING;
  }
  if (!strcmp(level, "info") || !strcmp(level, "default")
   || !strcmp(level, "")) {
    log_level = LOG_INFO;
  }
  if (!strcmp(level, "verbose")) {
    log_level = LOG_VERBOSE;
  }
  if (!strcmp(level, "debug")) {
    log_level = LOG_DEBUG;
  }
}