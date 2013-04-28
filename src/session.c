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

#include "log.h"
#include "strings.h"

#include <inttypes.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static session_t *sessions = NULL;
static int n_sessions = 0;
static pthread_mutex_t session_mutex = PTHREAD_MUTEX_INITIALIZER;

static session_t *get_session(const char *id)
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

static char *generate_session_id()
{
  char *id = NULL;

  do {
    free(id);
    id = stringf("%" PRIx64 "%x", (int64_t)time(NULL), rand());
  } while (get_session(id));
 
  return id;
}

static int purge_oldest_session()
{
  session_t *p, *oldest = NULL;
  for (p = sessions; p; p = p->next) {
    if (!oldest || (p->last_request < oldest->last_request && p->refs == 0)) {
      oldest = p;
    }
  }

  if (!oldest) {
    /* This block shouldn't execute without a bug related to dereferencing */
    musicd_log(LOG_WARNING, "session",
               "MAX_SESSIONS reached but all sessions in use");
    return -1;
  }

  musicd_log(LOG_DEBUG, "session", "MAX_SESSIONS reached, purging %s",
             oldest->id);

  if (oldest->prev) {
    oldest->prev->next = oldest->next;
  }
  if (oldest->next) {
    oldest->next->prev = oldest->prev;
  }
  if (sessions == oldest) {
    sessions = oldest->next;
  }

  free(oldest->id);
  free(oldest);

  --n_sessions;
  return 0;
}

session_t *session_new()
{
  pthread_mutex_lock(&session_mutex);

  while (n_sessions >= MAX_SESSIONS && !purge_oldest_session()) { }

  session_t *session = malloc(sizeof(session_t));
  memset(session, 0, sizeof(session_t));

  session->id = generate_session_id();
  session->refs = 1;

  musicd_log(LOG_DEBUG, "session", "new session %s", session->id);

  if (sessions) {
    session->next = sessions;
    sessions->prev = session;
  }
  sessions = session;

  ++n_sessions;

  pthread_mutex_unlock(&session_mutex);
  return session;
}

session_t *session_get(const char *id)
{  
  session_t *session;

  pthread_mutex_lock(&session_mutex);
  session = get_session(id);
  if (session) {
    ++session->refs;
  }
  pthread_mutex_unlock(&session_mutex);

  return session;
}

void session_deref(session_t *session)
{
  if (!session) {
    return;
  }
  pthread_mutex_lock(&session_mutex);
  --session->refs;
  pthread_mutex_unlock(&session_mutex);
}
