/*
 * This file is part of musicd.
 * Copyright (C) 2011-2013 Konsta Kokkinen <kray@tsundere.fi>
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

#ifndef MUSICD_SESSION_H
#define MUSICD_SESSION_H

#include <time.h>

typedef struct session {
  char *id;
  time_t last_request;

  /** User or NULL if share */
  char *user;

  struct session *next;
} session_t;


session_t *session_new();
session_t *session_get(const char *id);

#endif
