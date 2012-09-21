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

#include "config.h"
#include "libav.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

int log_level = LOG_INFO;
const char *log_time_format = "%H:%M:%S";

static void print
  (int level, const char * subsys, const char *fmt, va_list va_args)
{
  time_t now;
  char timestr[128];
  
  now = time(NULL);
  
  if (!strftime(timestr, sizeof(timestr), log_time_format, localtime(&now))) {
    timestr[0] = '\0';
  }
  
  if (level == LOG_ERROR) {
    fprintf(stderr, "\033[1;31;40m");
  }
  if (level == LOG_FATAL) {
    fprintf(stderr, "\033[0;1;41m");
  }
  
  fprintf(stderr, "%s [%s] ", timestr, subsys);
  vfprintf(stderr, fmt, va_args);
  
  if (level <= LOG_ERROR) {
    fprintf(stderr, "\033[0m");
  }

}

void musicd_log(int level, const char *subsys, const char *fmt, ...)
{
  va_list va_args;
  if (level > log_level) {
    return;
  }
  va_start(va_args, fmt);
  print(level, subsys, fmt, va_args);
  fprintf(stderr, "\n");
  va_end(va_args);
}

void musicd_perror(int level, const char *subsys, const char *fmt, ... )
{
  va_list va_args;
  if (level > log_level) {
    return;
  }
  va_start(va_args, fmt);
  print(level, subsys, fmt, va_args);
  fprintf(stderr, ": %s\n", strerror(errno));
  va_end(va_args);
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

void log_time_format_changed(char* format)
{
  log_time_format = format;
}

