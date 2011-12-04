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

#include <sqlite3.h>


int library_open()
{
  char *file;
  
  file = config_to_path("db-file");
  if (!file) {
    musicd_log(LOG_ERROR, "library", "db-file not set");
    return -1;
  }
  
  if (db_open(file)) {
    musicd_log(LOG_ERROR, "library", "Database couldn't be opened.");
    return -1;
  }
  
  return 0;
  
}

static bool prepare_query(const char *sql, sqlite3_stmt **query)
{
  if (sqlite3_prepare_v2(db_handle(), sql, -1, query, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s",
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
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'.",
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
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'.",
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


int64_t library_track_add(track_t *track, int64_t url)
{
  static const char *sql =
    "INSERT INTO tracks (url, track, title, artist, album, start, duration) VALUES(?, ?, ?, ?, ?, ?, ?)";
  
  int64_t artist, album;
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
    
  artist = field_rowid_create("artists", "name", track->artist);
  album = field_rowid_create("albums", "name", track->album);
  
  sqlite3_bind_int64(query, 1, url);
  sqlite3_bind_int(query, 2, track->track);
  sqlite3_bind_text(query, 3, track->title, -1, NULL);
  sqlite3_bind_int64(query, 4, artist);
  sqlite3_bind_int64(query, 5, album);
  sqlite3_bind_int(query, 6, track->start);
  sqlite3_bind_int(query, 7, track->duration);
  
  if (!execute(query)) {
    return -1;
  }

  return sqlite3_last_insert_rowid(db_handle());
}


int64_t library_url(const char* path, int64_t directory)
{
  static const char *sql =
    "INSERT INTO urls (path, directory) VALUES(?, ?)";
  
  sqlite3_stmt *query;
  int64_t result;
  
  result = field_rowid("urls", "path", path);
  
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
time_t library_url_mtime(int64_t url)
{
  static const char *sql = "SELECT mtime FROM urls WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return -1;
  }
  
  sqlite3_bind_int64(query, 1, url);

  return execute_scalar(query);
}
void library_url_mtime_set(int64_t url, time_t mtime)
{
  static const char *sql = "UPDATE urls SET mtime = ? WHERE rowid = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, mtime);
  sqlite3_bind_int64(query, 2, url);

  execute(query);
}

void library_iterate_urls_by_directory
  (int64_t directory, bool (*callback)(library_url_t *url))
{
  static const char *sql = "SELECT rowid, path, mtime, directory FROM urls WHERE directory = ?";
  sqlite3_stmt *query;
  int result;
  library_url_t url;
  bool cb_result = true;
  
  if (!prepare_query(sql, &query)) {
    return;
  }
  
  sqlite3_bind_int64(query, 1, directory);
  
  while ((result = sqlite3_step(query)) == SQLITE_ROW) {
    url.id = sqlite3_column_int64(query, 0);
    url.path = (const char*)sqlite3_column_text(query, 1);
    url.mtime = sqlite3_column_int64(query, 2);
    url.directory = sqlite3_column_int64(query, 3);
    
    cb_result = callback(&url);
    if (cb_result == false) {
      break;
    }
  }
  if (cb_result == true && result != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'.", sql);
  }
  
  sqlite3_finalize(query);
}

void library_url_clear(int64_t url)
{
  static const char *sql = "DELETE FROM tracks WHERE url = ?";
  sqlite3_stmt *query;
  
  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, url);
  
  execute(query);
}


void library_url_delete(int64_t url)
{
  static const char *sql = "DELETE FROM urls WHERE rowid = ?";
  sqlite3_stmt *query;
  
  library_url_clear(url);
  
  if (!prepare_query(sql, &query)) {
    return;
  }

  sqlite3_bind_int64(query, 1, url);
  
  execute(query);
}



int64_t library_directory(const char* path, int64_t parent)
{
  static const char *sql =
    "INSERT INTO directories (path, parent) VALUES(?, ?)";
  
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

void library_iterate_directories
  (int64_t parent, bool (*callback)(library_directory_t *directory))
{
  static const char *sql = "SELECT rowid, path, mtime, parent FROM directories WHERE parent = ?";
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
    
    cb_result = callback(&directory);
    if (cb_result == false) {
      break;
    }
  }
  if (cb_result == true && result != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library", "sqlite3_step failed for '%s'.", sql);
  }
  
  sqlite3_finalize(query);
}


struct library_query {
  library_table_t table;
  library_field_t field;
  sqlite3_stmt *stmt;
};

library_query_t *library_search
  (library_table_t table, library_field_t field, const char *search)
{
  sqlite3_stmt *result;
  
  static const char *q_track_all =
    "SELECT tracks.rowid AS id, urls.path AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE (COALESCE(tracks.title, '') || COALESCE(artists.name, '') || COALESCE(albums.name, '')) LIKE ?";
  
  /*static const char *q_track_title = 
    "SELECT tracks.rowid AS id, urls.path AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE COALESCE(tracks.title, '') LIKE ?";
               
  static const char *q_track_artist = 
    "SELECT tracks.rowid AS id, urls.path AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE COALESCE(artists.name, '') LIKE ?";

  static const char *q_track_album = 
    "SELECT tracks.rowid AS id, urls.path AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE COALESCE(albums.name, '') LIKE ?";
  */
  const char *sql = NULL;
  
  sql = q_track_all;
  
  
  /*if (table == LIBRARY_TABLE_TRACKS){
    if (field == LIBRARY_FIELD_ALL) {
      sql = q_track_all;
    } else if (field == LIBRARY_FIELD_TITLE) {
      sql = q_track_title;
    } else if (field == LIBRARY_FIELD_ARTIST) {
      sql = q_track_artist;
    } else if (field == LIBRARY_FIELD_ALBUM) {
      sql = q_track_album;
    }
  }*/
  if (sqlite3_prepare_v2(db_handle(), q_track_all, -1, &result, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s", sql, db_error());
    return NULL;
  }
  
  sqlite3_bind_text(result, 1, search, -1, NULL);
  
  return (library_query_t*)result;
}

int library_query_next(library_query_t *query, track_t* track)
{
  int result;
  
  if (!query) {
    return -1;
  }
  
  result = sqlite3_step((sqlite3_stmt*)query);
  if (result == SQLITE_DONE) {
    return 1;
  } else if (result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "library_search_next: sqlite3_step failed.");
    return -1;
  }
  
  track->id = sqlite3_column_int((sqlite3_stmt*)query, 0);
  track->path = (char*)sqlite3_column_text((sqlite3_stmt*)query, 1);
  track->track = sqlite3_column_int((sqlite3_stmt*)query, 2);
  track->title = (char*)sqlite3_column_text((sqlite3_stmt*)query, 3);
  track->artist = (char*)sqlite3_column_text((sqlite3_stmt*)query, 4);
  track->album = (char*)sqlite3_column_text((sqlite3_stmt*)query, 5);
  track->duration = sqlite3_column_int((sqlite3_stmt*)query, 6);
  return 0;
}

void library_query_close(library_query_t *query)
{
  sqlite3_finalize((sqlite3_stmt*)query);
}

/* Used by library_track_by_id. */
static char *dup_or_empty(const char *src)
{
  return strdup(src ? src : "");
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
    "SELECT tracks.rowid AS id, urls.path AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.start AS start, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE tracks.rowid = ?";
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s", sql,
               db_error());
    return NULL;
  }
  
  sqlite3_bind_int(stmt, 1, id);
  
  result = sqlite3_step(stmt);
  if (result == SQLITE_DONE) {
    return NULL;
  } else if (result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "library_track_by_id: sqlite3_step failed.");
    return NULL;
  }
  
  track = track_new();
  track->id = sqlite3_column_int(stmt, 0);
  track->path = dup_or_empty((const char*)sqlite3_column_text(stmt, 1));
  track->track = sqlite3_column_int(stmt, 2);
  track->title = dup_or_empty((const char*)sqlite3_column_text(stmt, 3));
  track->artist = dup_or_empty((const char*)sqlite3_column_text(stmt, 4));
  track->album = dup_or_empty((const char*)sqlite3_column_text(stmt, 5));
  track->start = sqlite3_column_int(stmt, 6);
  track->duration = sqlite3_column_int(stmt, 7);
  
  musicd_log(LOG_DEBUG, "library", "%i %s %i %s %s %s %i %i", track->id,
             track->path, track->track, track->title, track->artist,
             track->album, track->start, track->duration);
  
  return track;
}


