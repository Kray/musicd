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

#include <stdbool.h>
#include <time.h>

int library_open();

int64_t library_track_add(track_t *track, int64_t url);


/**
 * Returns id of url located by @p path. If it does not exist in the database,
 * it can be created depending on @p directory.
 * @param path  Url path.
 * @param directory Id of directory the url exists in. If > 0, a new url entry
 * will be created to the database if not found.
 * @returns Url id or 0 if not existing and @p directory <= 0. On error < 0.
 */
int64_t library_url(const char *path, int64_t directory);
/**
 * Removes all tracks associated with @p url from the database.
 */
void library_url_clear(int64_t url);
/**
 * Calls library_url_clear and removes the @p url entry from database.
 */
void library_url_delete(int64_t url);
/**
 * @Returns mtime of @p url
 */
time_t library_url_mtime(int64_t url);
/**
 * Sets mtime of @p url to @p mtime.
 */
void library_url_mtime_set(int64_t url, time_t mtime);

typedef struct {
  int64_t id;
  const char *path;
  time_t mtime;
  int64_t directory;
} library_url_t;
/**
 * Iterate through urls with directory @p directory. Stops when @p callback
 * returns false.
 */
void library_iterate_urls_by_directory
  (int64_t directory, bool (*callback)(library_url_t *url));


/**
 * Returns id of directory located by @p path. If it does not exist in the
 * database, it can be created depending on @p parent.
 * @param path Directory path.
 * @param parent Id of parent directory. If >= 0, a new directory entry will be
 * created to the database if not found.
 * @returns Directory id or 0 if not existing and @p parent < 0. On error < 0.
 */
int64_t library_directory(const char *path, int64_t parent);
/**
 * Recursively calls library_directory_delete for each directory with parent
 * @p directory in database and library_url_delete for each url with directory
 * @p directory.
 */
void library_directory_delete(int64_t directory);
/**
 * @Returns mtime of @p directory.
 */
time_t library_directory_mtime(int64_t directory);
/**
 * Sets mtime of @p directory to @p mtime.
 */
void library_directory_mtime_set(int64_t directory, time_t mtime);

typedef struct {
  int64_t id;
  const char *path;
  time_t mtime;
  int64_t parent;
} library_directory_t;

/**
 * Iterate through directories with parent @p parent. Stops when @p callback
 * returns false.
 */
void library_iterate_directories
  (int64_t parent, bool (*callback)(library_directory_t *directory));


int64_t library_image_add(int64_t url);

typedef struct {
  int64_t id;
  const char *path;
  int64_t directory;
  int64_t album;
} library_image_t;

void library_iterate_images_by_directory
  (int64_t directory, bool (*callback)(library_image_t *image));


/**
 * @Returns most common album of tracks in urls located in @p directory.
 */
int64_t library_album_by_directory(int64_t directory);

/**
 * Sets album to @p album of all images located in @p directory.
 */
void library_image_album_set_by_directory(int64_t directory, int64_t album);


typedef struct library_query library_query_t;

typedef enum {
  LIBRARY_TABLE_NONE,
  LIBRARY_TABLE_TRACKS,
  LIBRARY_TABLE_ARTISTS,
  LIBRARY_TABLE_ALBUMS,
} library_table_t;

typedef enum {
  LIBRARY_FIELD_NONE,
  LIBRARY_FIELD_URL,
  LIBRARY_FIELD_TITLE,
  LIBRARY_FIELD_ARTIST,
  LIBRARY_FIELD_ALBUM,
  LIBRARY_FIELD_ALL
} library_field_t;

/**
 * Starts a query from the library.
 * @param table Table to be queried.
 * @param field Field in the table to be searched from. NONE means no
 * filtering, ALL search from all fields. Note that obviously all fields are
 * not available in all tables.
 * @param search Search string. If NULL, no filtering will be done. 
 * @todo FIXME New search API not ready yet.
 */
library_query_t *library_search(library_table_t table, library_field_t field,
                                const char *search);

/*library_query_t *library_search(const char *search);*/

int library_query_next(library_query_t *query, track_t *track);
void library_query_close(library_query_t *query);



track_t *library_track_by_id(int64_t id);

#endif
