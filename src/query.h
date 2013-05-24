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
#ifndef MUSICD_QUERY_H
#define MUSICD_QUERY_H

#include "track.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  QUERY_FIELD_NONE = 0,

  QUERY_FIELD_TRACKID,
  QUERY_FIELD_ARTISTID,
  QUERY_FIELD_ALBUMID,

  QUERY_FIELD_TITLE,
  QUERY_FIELD_ARTIST,
  QUERY_FIELD_ALBUM,
  QUERY_FIELD_TRACK,
  QUERY_FIELD_DURATION,
  QUERY_FIELD_ALL,
} query_field_t;

query_field_t query_field_from_string(const char *string);

typedef struct query query_t;

query_t *query_tracks_new();
query_t *query_artists_new();
query_t *query_albums_new();

void query_close(query_t *query);

/**
 * Applies filter @p filter in @p field.
 * If the field is an id field, the value can be a comma-separated list.
 * @note Only one filter per field.
 */
void query_filter(query_t *query, query_field_t field,
                          const char *filter);

/**
 * Limits @p query to return only @p limit rows. Negative value means no limit.
 */
void query_limit(query_t *query, int64_t limit);

/**
 * Offsets @p query by @p offset rows.
 */
void query_offset(query_t *query, int64_t offset);

/**
 * Adds sorting rule on @p query by @p field. @p descending changes sorting
 * direction.
 */
void query_sort(query_t *query, query_field_t field,
                        bool descending);

/**
 * Parses sorting rules from @p sort
 * Format:
 *   Comma-separated list of field names. Prefix '-' indicates descending
 *   sorting order while ascending is default.
 * Example:
 *   -artist,album,track
 * @returns 0 on success, nonzero on error
 */
int query_sort_from_string(query_t *query, const char *sort);

/**
 * @returns amount of results returned by the current filters.
 */
int64_t query_count(query_t *query);

/**
 * @returns <0 on error, 0 if not found; positive value is index + 1
 */
int64_t query_index(query_t *query, int64_t id);

/**
 * Starts the query. After calling this filters or sorting can't be
 * modified anymore.
 */
int query_start(query_t *query);

int query_tracks_next(query_t *query, track_t *track);

typedef struct {
  int64_t artistid;
  char *artist;
} query_artist_t;
int query_artists_next(query_t *query, query_artist_t *artist);

typedef struct {
  int64_t albumid;
  char *album;
} query_album_t;
int query_albums_next(query_t *query, query_album_t *album);


#endif

