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

#define MAX_SESSIONS 10000

typedef struct session {
  char *id;
  time_t last_request;

  /** User or NULL if share */
  char *user;

  int refs;

  struct session *prev, *next;
} session_t;


/**
 * @return New session with random id and reference counter of 1
 */
session_t *session_new();

/**
 * Raises reference counter by one
 * @return NULL if not found
 */
session_t *session_get(const char *id);

/**
 * Must be called when done with the session to prevent it hanging around when
 * it would be deleted
 */
void session_deref(session_t *session);

#endif
