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
#include "library.h"

#include "config.h"
#include "cue.h"
#include "db.h"
#include "log.h"
#include "strings.h"

#include <sqlite3.h>


int library_open()
{
  return 0;
}

static bool prepare_query(const char *sql, sqlite3_stmt **query)
{
  if (sqlite3_prepare_v2(db_handle(), sql, -1, query, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "can't prepare '%s': %s",
               sql, db_error());
    return false;
  }
  return true;
}

static bool execute(sqlite3_stmt *query)
{
  int result = sqlite3_step(query);
  if (result == SQLITE_DONE || result == SQLITE_ROW) {
    result = true;
  } else {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'",
               sqlite3_sql(query));
    result = false;
  }
  sqlite3_finalize(query);
  return result;
}

static int64_t execute_scalar(sqlite3_stmt *query)
{
  int64_t result = sqlite3_step(query);
  if (result == SQLITE_ROW) {
    result = sqlite3_column_int(query, 0);
  } else if (result == SQLITE_DONE) {
    result = 0;
  } else {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'",
               sqlite3_sql(query));
    result = -1;
  }
  sqlite3_finalize(query);
  return result;
}

static int64_t field_rowid(const char *table, const char *field, const char *value)
{
  sqlite3_stmt *query;
  int64_t result;
  char *sql = malloc(strlen(table) + strlen(field) + 32);
  
  sprintf(sql, "SELECT rowid FROM %s WHERE %s = ?", table, field);
  
  if (!prepare_query(sql, &query)) {
    result = -1;
  } else {
    sqlite3_bind_text(query, 1, value, -1, NULL);
    result = execute_scalar(query);
  }

  free(sql);
  return result;
}

static int64_t field_rowid_create(const char *table, const char *field, const char *value)
{
  sqlite3_stmt *query;
  int64_t result;
  char *sql;
  
  result = field_rowid(table, field, value);
  if (result > 0) {
    return result;
  }
  
  sql = malloc(strlen(table) + strlen(field) + 32);
  
  sprintf(sql, "INSERT INTO %s (%s) VALUES (?)", table, field);
  if (!prepare_query(sql, &query)) {
    result = -1;
  } else {
    sqlite3_bind_text(query, 1, value, -1, NULL);
    result = execute_scalar(query);
  }
  
  free(sql);
  return sqlite3_last_insert_rowid(db_handle());
}

static void increment_album_tracks(int64_t album)
{
  static const char *sql =
    "UPDATE albums SET tracks = tracks + 1 WHERE rowid = ?";

  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, album);

  execute(query);
}

char *library_root_path()
{
  static const char *sql =
    "SELECT path FROM directories WHERE parentid = 0";
  sqlite3_stmt *query;
  int result;
  char *path = NULL;;

  if (!prepare_query(sql, &query)) {
    return NULL;
  }

  result = sqlite3_step(query);
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  if (result == SQLITE_ROW) {
    path = strcopy((const char *)sqlite3_column_text(query, 0));
  }

  sqlite3_finalize(query);
  return path;
}

int64_t library_track_add(track_t *track, int64_t directory)
{
  static const char *sql =
    "INSERT INTO tracks (fileid, file, cuefileid, cuefile, track, title, artistid, artist, albumid, album, start, duration, track_index) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return -1;
  }

  track->fileid = library_file(track->file, directory);
  if (track->cuefile) {
    track->cuefileid = library_file(track->cuefile, directory);
  }

  if (track->artist) {
    track->artistid = field_rowid_create("artists", "name", track->artist);
  }
  if (track->album) {
    track->albumid = field_rowid_create("albums", "name", track->album);
  }

  sqlite3_bind_int64(query, 1, track->fileid);
  sqlite3_bind_text(query, 2, track->file, -1, NULL);
  sqlite3_bind_int64(query, 3, track->cuefileid);
  sqlite3_bind_text(query, 4, track->cuefile, -1, NULL);
  sqlite3_bind_int(query, 5, track->track);
  sqlite3_bind_text(query, 6, track->title, -1, NULL);
  sqlite3_bind_int64(query, 7, track->artistid);
  sqlite3_bind_text(query, 8, track->artist, -1, NULL);
  sqlite3_bind_int64(query, 9, track->albumid);
  sqlite3_bind_text(query, 10, track->album, -1, NULL);
  sqlite3_bind_double(query, 11, track->start);
  sqlite3_bind_double(query, 12, track->duration);
  sqlite3_bind_double(query, 13, track->track_index);

  if (!execute(query)) {
    return -1;
  }

  if (track->album) {
    increment_album_tracks(track->albumid);
  }

  return sqlite3_last_insert_rowid(db_handle());
}


int64_t library_file(const char* path, int64_t directory)
{
  static const char *sql =
    "INSERT INTO files (path, directoryid) VALUES(?, ?)";
  
  sqlite3_stmt *query;
  int64_t result;
  
  result = field_rowid("files", "path", path);
  
  /* Result is nonzero (found or error) or directory <= 0. */
  if (result != 0 || directory <= 0) {
    return result;
  }
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_text(query, 1, path, -1, NULL);
  sqlite3_bind_int64(query, 2, directory);
  result = execute_scalar(query);
 
  return result ? result : sqlite3_last_insert_rowid(db_handle());
}
time_t library_file_mtime(int64_t file)
{
  static const char *sql = "SELECT mtime FROM files WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_int64(query, 1, file);

  return execute_scalar(query);
}
void library_file_mtime_set(int64_t file, time_t mtime)
{
  static const char *sql = "UPDATE files SET mtime = ? WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, mtime);
  sqlite3_bind_int64(query, 2, file);

  execute(query);
}

void library_iterate_files_by_directory
  (int64_t directory, bool (*callback)(library_file_t *file))
{
  static const char *sql = "SELECT rowid, path, mtime, directoryid FROM files WHERE directoryid = ?";
  sqlite3_stmt *query;
  int result;
  library_file_t file;
  bool cb_result = true;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, directory);
  
  while ((result = sqlite3_step(query)) == SQLITE_ROW) {
    file.id = sqlite3_column_int64(query, 0);
    file.path = (const char*)sqlite3_column_text(query, 1);
    file.mtime = sqlite3_column_int64(query, 2);
    file.directory = sqlite3_column_int64(query, 3);
    
    cb_result = callback(&file);
    if (cb_result == false) {
      break;
    }
  }
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  
  sqlite3_finalize(query);
}

void library_file_clear(int64_t file)
{
  static const char *sql_album_tracks =
    "UPDATE albums SET tracks = (SELECT COUNT(tracks.rowid) FROM tracks WHERE tracks.albumid = albums.rowid AND tracks.fileid != ?1) WHERE albums.rowid IN (SELECT albumid FROM tracks WHERE fileid = ?1)";
  static const char *sql_tracks = "DELETE FROM tracks WHERE fileid = ?";
  static const char *sql_images = "DELETE FROM images WHERE fileid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql_album_tracks, &query)) {
    return;
  }
  sqlite3_bind_int64(query, 1, file);
  execute(query);

  if (!prepare_query(sql_tracks, &query)) {
    return;
  }
  sqlite3_bind_int64(query, 1, file);
  execute(query);
  
  if (!prepare_query(sql_images, &query)) {
    return;
  }
  sqlite3_bind_int64(query, 1, file);
  execute(query);
}


void library_file_delete(int64_t file)
{
  static const char *sql = "DELETE FROM files WHERE rowid = ?";
  sqlite3_stmt *query;
  
  library_file_clear(file);
  
  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, file);
  
  execute(query);
}



int64_t library_directory(const char* path, int64_t parent)
{
  static const char *sql =
    "INSERT INTO directories (path, parentid) VALUES(?, ?)";
  
  sqlite3_stmt *query;
  int64_t result;
  
  result = field_rowid("directories", "path", path);
  
  /* Result is nonzero (found or error) or parent < 0. */
  if (result != 0 || parent < 0) {
    return result;
  }
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_text(query, 1, path, -1, NULL);
  sqlite3_bind_int64(query, 2, parent);
  result = execute_scalar(query);

  return result ? result : sqlite3_last_insert_rowid(db_handle());
}
char *library_directory_path(int64_t directory)
{
  static const char *sql =
    "SELECT path FROM directories WHERE rowid = ?";
  sqlite3_stmt *query;
  int result;
  char *path = NULL;;

  if (!prepare_query(sql, &query)) {
    return NULL;
  }

  sqlite3_bind_int64(query, 1, directory);

  result = sqlite3_step(query);
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  if (result == SQLITE_ROW) {
    path = strcopy((const char *)sqlite3_column_text(query, 0));
  }

  sqlite3_finalize(query);
  return path;
}
static bool delete_files_cb(library_file_t *file)
{
  library_file_delete(file->id);
  return true;
}
static bool delete_directories_cb(library_directory_t *directory, void *empty)
{
  (void)empty;
  library_directory_delete(directory->id);
  return true;
}
void library_directory_delete(int64_t directory)
{
  static const char *sql = "DELETE FROM directories WHERE rowid = ?";
  sqlite3_stmt *query;
  
  library_iterate_files_by_directory(directory, delete_files_cb);
  library_iterate_directories(directory, delete_directories_cb, NULL);
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, directory);
  execute(query);
}
time_t library_directory_mtime(int64_t directory)
{
  static const char *sql = "SELECT mtime FROM directories WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_int64(query, 1, directory);

  return execute_scalar(query);
}
void library_directory_mtime_set(int64_t directory, time_t mtime)
{
  static const char *sql = "UPDATE directories SET mtime = ? WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, mtime);
  sqlite3_bind_int64(query, 2, directory);

  execute(query);
}
int library_directory_tracks_count(int64_t directory)
{
  static const char *sql = "SELECT COUNT(tracks.rowid) FROM directories JOIN files ON files.directoryid = directories.rowid JOIN tracks ON tracks.fileid = files.rowid WHERE directories.rowid = ?";
  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return -1;
  }

  sqlite3_bind_int64(query, 1, directory);

  return execute_scalar(query);
}

void library_iterate_directories
  (int64_t parent,
   bool (*callback)(library_directory_t *directory, void *opaque),
   void *opaque)
{
  static const char *sql = "SELECT rowid, path, mtime, parentid FROM directories WHERE parentid = ?";
  sqlite3_stmt *query;
  int result;
  library_directory_t directory;
  bool cb_result;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, parent);
  
  while ((result = sqlite3_step(query)) == SQLITE_ROW) {
    directory.id = sqlite3_column_int64(query, 0);
    directory.path = (const char*)sqlite3_column_text(query, 1);
    directory.mtime = sqlite3_column_int64(query, 2);
    directory.parent = sqlite3_column_int64(query, 3);
    
    cb_result = callback(&directory, opaque);
    if (cb_result == false) {
      break;
    }
  }
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  
  sqlite3_finalize(query);
}


int64_t library_image_add(int64_t file)
{
  static const char *sql =
    "INSERT INTO images (fileid) VALUES(?)";
  
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_int64(query, 1, file);
  
  if (!execute(query)) {
    return -1;
  }
  
  return sqlite3_last_insert_rowid(db_handle());
}

char *library_image_path(int64_t image)
{
  static const char *sql =
    "SELECT files.path AS path FROM images JOIN files ON images.fileid = files.rowid WHERE images.rowid = ?";
  sqlite3_stmt *query;
  int result;
  char *path = NULL;;

  if (!prepare_query(sql, &query)) {
    return NULL;
  }

  sqlite3_bind_int64(query, 1, image);

  result = sqlite3_step(query);
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  if (result == SQLITE_ROW) {
    path = strcopy((const char *)sqlite3_column_text(query, 0));
  }

  sqlite3_finalize(query);
  return path;
}

int64_t library_album_image(int64_t album)
{
  static const char *sql =
    "SELECT imageid FROM albums WHERE rowid = ?";
  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return 0;
  }

  sqlite3_bind_int64(query, 1, album);

  return execute_scalar(query);
}

void library_album_image_set(int64_t album, int64_t image)
{
  static const char *sql = "UPDATE albums SET imageid = ? WHERE rowid = ?";
  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, image);
  sqlite3_bind_int64(query, 2, album);

  execute(query);
}

void library_iterate_images_by_directory
  (int64_t directory, bool (*callback)(library_image_t *file))
{
  static const char *sql =
    "SELECT images.rowid AS id, files.path AS path, images.albumid AS albumid FROM files JOIN images ON images.fileid = files.rowid WHERE files.directoryid = ?;";
  sqlite3_stmt *query;
  int result;
  library_image_t image;
  bool cb_result = true;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, directory);
  
  image.directory = directory;
  
  while ((result = sqlite3_step(query)) == SQLITE_ROW) {
    image.id = sqlite3_column_int64(query, 0);
    image.path = (const char*)sqlite3_column_text(query, 1);
    image.album = sqlite3_column_int64(query, 2);
    
    cb_result = callback(&image);
    if (cb_result == false) {
      break;
    }
  }
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  
  sqlite3_finalize(query);
}


void library_iterate_images_by_album
  (int64_t album, bool (*callback)(library_image_t *file, void *opaque), void *opaque)
{
  static const char *sql =
    "SELECT images.rowid AS id, files.path AS path, files.directoryid AS directoryid FROM images JOIN files ON images.fileid = files.rowid WHERE images.albumid = ?;";
  sqlite3_stmt *query;
  int result;
  library_image_t image;
  bool cb_result = true;

  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, album);

  image.album = album;

  while ((result = sqlite3_step(query)) == SQLITE_ROW) {
    image.id = sqlite3_column_int64(query, 0);
    image.path = (const char*)sqlite3_column_text(query, 1);
    image.directory = sqlite3_column_int64(query, 2);

    cb_result = callback(&image, opaque);
    if (cb_result == false) {
      break;
    }
  }
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }

  sqlite3_finalize(query);
}


int64_t library_album_by_directory(int64_t directory)
{
  static const char *sql =
    "SELECT tracks.albumid FROM directories JOIN files ON files.directoryid = directories.rowid JOIN tracks ON tracks.fileid = files.rowid WHERE directories.rowid = ? GROUP BY tracks.albumid ORDER BY COUNT(tracks.albumid) DESC LIMIT 1";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return 0;
  }
  
  sqlite3_bind_int64(query, 1, directory);
  
  return execute_scalar(query);
}

void library_image_album_set_by_directory(int64_t directory, int64_t album)
{
  static const char *sql =
    "UPDATE images SET albumid = ? WHERE fileid IN (SELECT rowid FROM files WHERE directoryid = ?)";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, album);
  sqlite3_bind_int64(query, 2, directory);
  
  execute(query);
}

lyrics_t *library_lyrics(int64_t track, time_t *time)
{
  static const char *sql =
    "SELECT lyrics, provider, source, mtime FROM lyrics WHERE trackid = ?";
  sqlite3_stmt *query;
  int result;
  lyrics_t *lyrics;

  if (time) {
    *time = 0;
  }

  if (!prepare_query(sql, &query)) {
    return NULL;
  }
  
  sqlite3_bind_int64(query, 1, track);
  
  result = sqlite3_step(query);
  if (result != SQLITE_DONE && result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'", sql);
  }
  if (result == SQLITE_ROW) {
    if (time) {
      *time = sqlite3_column_int64(query, 3);
    }
    if (!sqlite3_column_text(query, 0)) {
      return NULL;
    }

    lyrics = lyrics_new();
    lyrics->lyrics = strcopy((const char *)sqlite3_column_text(query, 0));
    if (sqlite3_column_text(query, 1)) {
      lyrics->provider = strcopy((const char *)sqlite3_column_text(query, 1));
    }
    if (sqlite3_column_text(query, 2)) {
      lyrics->source = strcopy((const char *)sqlite3_column_text(query, 2));
    }
    return lyrics;
  }
  return NULL;
}

void library_lyrics_set(int64_t track, lyrics_t *lyrics)
{
  static const char *sql =
    "INSERT OR REPLACE INTO lyrics (trackid, lyrics, provider, source, mtime) VALUES(?, ?, ?, ?, ?)";
  sqlite3_stmt *query;

  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, track);
  sqlite3_bind_text(query, 2, lyrics ? lyrics->lyrics : NULL, -1, NULL);
  sqlite3_bind_text(query, 3, lyrics ? lyrics->provider : NULL, -1, NULL);
  sqlite3_bind_text(query, 4, lyrics ? lyrics->source : NULL, -1, NULL);
  sqlite3_bind_int64(query, 5, time(NULL));

  execute(query);
}

/**
 * @todo FIXME More or less ugly value duplication.
 */
track_t *library_track_by_id(int64_t id)
{
  sqlite3_stmt *stmt;
  track_t *track;
  int result;

  static const char *sql =
    "SELECT rowid AS id, fileid, file, cuefileid, cuefile, track, title, artistid, artist, albumid, album, start, duration, track_index FROM tracks WHERE rowid = ?";

  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "can't prepare '%s': %s", sql,
               db_error());
    return NULL;
  }

  sqlite3_bind_int(stmt, 1, id);

  result = sqlite3_step(stmt);
  if (result == SQLITE_DONE) {
    return NULL;
  } else if (result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "library_track_by_id: sqlite3_step failed");
    return NULL;
  }

  track = track_new();
  track->id = sqlite3_column_int64(stmt, 0);
  track->fileid = sqlite3_column_int64(stmt, 1);
  track->file = strcopy((const char *)sqlite3_column_text(stmt, 2));
  track->cuefileid = sqlite3_column_int64(stmt, 3);
  track->cuefile = strcopy((const char *)sqlite3_column_text(stmt, 4));
  track->track = sqlite3_column_int(stmt, 5);
  track->title = strcopy((const char *)sqlite3_column_text(stmt, 6));
  track->artistid = sqlite3_column_int64(stmt, 7);
  track->artist = strcopy((const char *)sqlite3_column_text(stmt, 8));
  track->albumid = sqlite3_column_int64(stmt, 9);
  track->album = strcopy((const char *)sqlite3_column_text(stmt, 10));
  track->start = sqlite3_column_double(stmt, 11);
  track->duration = sqlite3_column_double(stmt, 12);
  track->track_index = sqlite3_column_double(stmt, 13);

  /*musicd_log(LOG_DEBUG, "library", "%" PRId64 " %s %s %d %s %s %s %lf %lf",
             track->id, track->file, track->cuefile, track->track, track->title, track->artist,
             track->album, track->start, track->duration);*/

  sqlite3_finalize(stmt);

  return track;
}

int64_t library_tracks_total()
{
  sqlite3_stmt *query;
  if (!prepare_query("SELECT COUNT(rowid) FROM tracks", &query)) {
    return 0;
  }
  return execute_scalar(query);
}

int64_t library_randomid()
{
  sqlite3_stmt *query;
  if (!prepare_query("SELECT rowid FROM tracks ORDER BY RANDOM() LIMIT 1", &query)) {
    return 0;
  }
  return execute_scalar(query);
}
