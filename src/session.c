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

#include "session.h"

#include "strings.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static session_t *sessions = NULL;
static int n_sessions = 0;

static char *generate_session_id()
{
  char *id = NULL;

  do {
    free(id);
    id = stringf("%" PRIx64 "%x", (int64_t)time(NULL), rand());
  } while (session_get(id));
 
  return id;
}

session_t *session_new()
{
  session_t *session = malloc(sizeof(session_t));
  memset(session, 0, sizeof(session_t));

  session->id = generate_session_id();

  if (sessions) {
    session->next = sessions;
  }
  sessions = session;

  ++n_sessions;

  return session;
}

session_t *session_get(const char *id)
{  
  session_t *session;

  if (!sessions) {
    return NULL;
  }

  for (session = sessions; session != NULL; session = session->next) {
    if (!strcmp(session->id, id)) {
      session->last_request = time(NULL);
      return session;
    }
  }
  return NULL;
}
