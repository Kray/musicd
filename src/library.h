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

/**
 * @returns amount of tracks in @p directory
 */
int library_directory_tracks_count(int64_t directory);

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
  (int64_t parent,
   bool (*callback)(library_directory_t *directory, void *opaque),
   void *opaque);


int64_t library_image_add(int64_t url);

typedef struct {
  int64_t id;
  const char *path;
  int64_t directory;
  int64_t album;
} library_image_t;

/**
 * @returns path which must be freed or NULL if not found.
 */
char *library_image_path(int64_t image);

int64_t library_album_image(int64_t album);
void library_album_image_set(int64_t album, int64_t image);

void library_iterate_images_by_directory
  (int64_t directory, bool (*callback)(library_image_t *image));

void library_iterate_images_by_album
  (int64_t album,
   bool (*callback)(library_image_t *image, void *opaque),
   void *opaque);


/**
 * @Returns most common album of tracks in urls located in @p directory.
 */
int64_t library_album_by_directory(int64_t directory);

/**
 * Sets album to @p album of all images located in @p directory.
 */
void library_image_album_set_by_directory(int64_t directory, int64_t album);



/**
 * @Returns lyrics of @p track or NULL if not found.
 * If @p time is not NULL, *time will be set to lyricstime of @p track
 */
char *library_lyrics(int64_t track, time_t *time);

/**
 * Sets lyrics of @p track to @p lyrics. Timestamp is automatically modified.
 */ 
void library_lyrics_set(int64_t track, char *lyrics);

track_t *library_track_by_id(int64_t id);

int64_t library_randomid();

#endif
