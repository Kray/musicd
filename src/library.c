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


/**
 * @returns first rowid from @p table where @p field = @p value. If no such row
 * exists, one will be created.
 */
static int field_id(const char *table, const char *field, const char *value)
{
  sqlite3_stmt *stmt;
  int result;
  char sql[strlen(table) + strlen(field) + 64];
  
  sprintf(sql, "SELECT rowid FROM %s WHERE %s = ?", table, field);
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return -1;
  }
  
  sqlite3_bind_text(stmt, 1, value, -1, NULL);
  
  result = sqlite3_step(stmt);
  
  if (result == SQLITE_ROW) {
    result = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
    return result;
  } else if (result != SQLITE_DONE) {
    /* We should actually never get here, but trying further doesn't hurt. */
    musicd_log(LOG_WARNING, "library", "field_id: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
  
  sprintf(sql, "INSERT INTO %s (%s) VALUES (?)", table, field);
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return 0;
  }

  sqlite3_bind_text(stmt, 1, value, -1, NULL);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    /* Ok, now it really is an error. */
    musicd_log(LOG_ERROR, "library", "field_id: sqlite3_step failed for value '%s'.", value);
    return 0;
  }

  result = sqlite3_last_insert_rowid(db_handle());
  sqlite3_finalize(stmt);
  
  return result;
}


time_t library_get_url_mtime(const char *url)
{
  sqlite3_stmt *stmt;
  int result;
  static const char *sql = "SELECT mtime FROM urls WHERE url = ?";
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return -1;
  }
  
  sqlite3_bind_text(stmt, 1, url, -1, NULL);
  
  result = sqlite3_step(stmt);
  if (result == SQLITE_DONE) {
    return 0;
  } else if(result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "get_url_mtime: sqlite3_step failed.");
    return 0;
  }
  
  result = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  return result;
}

void library_set_url_mtime(const char *url, time_t mtime)
{
  sqlite3_stmt *stmt;
  int id;
  int result;
  static const char *sql = "UPDATE urls SET mtime = ? WHERE rowid = ?";
  
  id = field_id("urls", "url", url);
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return;
  }
  
  sqlite3_bind_int64(stmt, 1, mtime);
  sqlite3_bind_int64(stmt, 2, id);
  
  result = sqlite3_step(stmt);
  if (result != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library", "set_url_mtime: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
}

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


int library_add(track_t *track)
{
  int url, artist, album;
  sqlite3_stmt *stmt;
  
  static const char *sql =
    "INSERT INTO tracks (url, track, title, artist, album, start, duration) VALUES(?, ?, ?, ?, ?, ?, ?)";

  /*musicd_log(LOG_DEBUG, "library", "%s %d %s", track->url, track->number, track->name);*/
  
  url = field_id("urls", "url", track->url);
  artist = field_id("artists", "name", track->artist);
  album = field_id("albums", "name", track->album);
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return -1;
  }
  
  sqlite3_bind_int(stmt, 1, url);
  sqlite3_bind_int(stmt, 2, track->track);
  sqlite3_bind_text(stmt, 3, track->title, -1, NULL);
  sqlite3_bind_int(stmt, 4, artist);
  sqlite3_bind_int(stmt, 5, album);
  sqlite3_bind_int(stmt, 6, track->start);
  sqlite3_bind_int(stmt, 7, track->duration);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library", "library_add: sqlite3_step failed.");
    sqlite3_finalize(stmt);
    return -1;
  }
  
  sqlite3_finalize(stmt);
  return 0;
}


void library_clear_url(const char *url)
{
  sqlite3_stmt *stmt;

  static const char *sql =
    "DELETE FROM tracks WHERE url = (SELECT rowid FROM urls WHERE url = ? LIMIT 1)";
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, db_error());
    return;
  }

  sqlite3_bind_text(stmt, 1, url, -1, NULL);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library",
               "library_clear_url: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
}


void library_delete_url(const char *url)
{
  sqlite3_stmt *stmt;

  static const char *sql = "DELETE FROM urls WHERE url = ?";
  
  library_clear_url(url);
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql,
               db_error());
    return;
  }

  sqlite3_bind_text(stmt, 1, url, -1, NULL);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library",
               "library_clear_url: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
}

void library_iterate_urls(bool (*callback)(const char *url))
{
  sqlite3_stmt *stmt;
  static const char *sql = "SELECT url FROM urls";
  int result;
  const char *url;
  bool cb_result;
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql,
               db_error());
    return;
  }
  
  while ((result = sqlite3_step(stmt)) == SQLITE_ROW) {
    url = (const char*)sqlite3_column_text(stmt, 0);
    
    cb_result = callback(url);
    if (cb_result == false) {
      break;
    }
  }
  if (result != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library",
               "library_iterate_urls: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
}


/*struct library_query_t {
  sqlite3_stmt *stmt;
};*/

library_query_t *library_search(const char *search)
{
  sqlite3_stmt *result;
  static const char *sql =
    "SELECT tracks.rowid AS id, urls.url AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE (COALESCE(tracks.title, '') || COALESCE(artists.name, '') || COALESCE(albums.name, '')) LIKE ?";
  
  if (sqlite3_prepare_v2(db_handle(), sql, -1, &result, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s", sql, db_error());
    return NULL;
  }
  
  sqlite3_bind_text(result, 1, search, -1, NULL);
  /*sqlite3_bind_text(result, 2, search, -1, NULL);
  sqlite3_bind_text(result, 3, search, -1, NULL);*/
  
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
  track->url = (char*)sqlite3_column_text((sqlite3_stmt*)query, 1);
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
track_t *library_track_by_id(int id)
{
  sqlite3_stmt *stmt;
  track_t *track;
  int result;
  static const char *sql =
    "SELECT tracks.rowid AS id, urls.url AS url, tracks.track AS track, tracks.title AS title, artists.name AS artist, albums.name AS album, tracks.start AS start, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE tracks.rowid = ?";
  
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
  track->url = dup_or_empty((const char*)sqlite3_column_text(stmt, 1));
  track->track = sqlite3_column_int(stmt, 2);
  track->title = dup_or_empty((const char*)sqlite3_column_text(stmt, 3));
  track->artist = dup_or_empty((const char*)sqlite3_column_text(stmt, 4));
  track->album = dup_or_empty((const char*)sqlite3_column_text(stmt, 5));
  track->start = sqlite3_column_int(stmt, 6);
  track->duration = sqlite3_column_int(stmt, 7);
  
  musicd_log(LOG_DEBUG, "library", "%i %s %i %s %s %s %i %i", track->id,
             track->url, track->track, track->title, track->artist,
             track->album, track->start, track->duration);
  
  return track;
}


