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
#ifndef MUSICD_LIBRARY_H
#define MUSICD_LIBRARY_H

#include "track.h"

#include <time.h>

int library_open();
void library_scan(const char *directory);

int library_add(track_t *track);

/**
 * If tracks with such url exist, delete them.
 */
void library_clear_url(const char *url);

#if 0
typedef enum {
  LIBRARY_FIELD_NONE,
  LIBRARY_FIELD_ALBUMARTIST,
  LIBRARY_FIELD_ARTIST,
  LIBRARY_FIELD_ALBUM,
  LIBRARY_FIELD_TRACK,
} library_field_t;

typedef struct {
  library_field_t type;
  int value;
} library_group_t;


typedef struct {
  library_field_t type;
  
  const char *name;
} library_entry_t;

typedef struct {
  library_field_t field;
  char *filter;
  
  void *internal; /**< Internal field used by library. */
} library_filter_t;


typedef struct library_list_t library_list_t;

/**
 * Starts a query with results being grouped by list in @p grouping. All
 * results will match @p filters, and at least one component in all results
 * will match @p search.
 * @param grouping Grouping for results, terminated by _NONE.
 * @param filters Field filters, terminated by type _NONE. May be NULL.
 * @param search Search spanning all fields. May be NULL.
 */
library_list_t *library_list(library_group_t *grouping, library_filter_t *filters, const char *search);

int library_list_next(library_list_t *list, library_entry_t *entry);
#endif

typedef struct library_query_t library_query_t;


library_query_t *library_search(const char *search);
int library_search_next(library_query_t *query, track_t *track);
void library_search_close(library_query_t *query);

track_t *library_track_by_id(int id);


time_t library_get_url_mtime(const char *url);
void library_set_url_mtime(const char *url, time_t mtime);

#endif
