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
#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <sqlite3.h>

static sqlite3 *db;

static void simple_exec(const char *sql, int *error)
{
  int result = sqlite3_exec(db, sql, NULL, NULL, NULL);
  if (result != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
    *error = result;
  }
}

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
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
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
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
    return 0;
  }

  sqlite3_bind_text(stmt, 1, value, -1, NULL);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    /* Ok, now it really is an error. */
    musicd_log(LOG_ERROR, "library", "field_id: sqlite3_step failed for value '%s'.", value);
    return 0;
  }

  result = sqlite3_last_insert_rowid(db);
  sqlite3_finalize(stmt);
  
  return result;
}


time_t library_get_url_mtime(const char *url)
{
  sqlite3_stmt *stmt;
  int result;
  static const char *sql = "SELECT mtime FROM urls WHERE url = ?";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
    return -1;
  }
  
  sqlite3_bind_text(stmt, 1, url, -1, NULL);
  
  result = sqlite3_step(stmt);
  if (result == SQLITE_DONE) {
    field_id("urls", "url", url);
    return 0;
  } else if(result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "library", "get_url_mtime: sqlite3_step failed.");
  }
  
  result = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);
  
  return result;
}

void library_set_url_mtime(const char *url, time_t mtime)
{
  sqlite3_stmt *stmt;
  int result;
  static const char *sql = "UPDATE urls SET mtime = ? WHERE url = ?";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
    return;
  }
  
  sqlite3_bind_int64(stmt, 1, mtime);
  sqlite3_bind_text(stmt, 2, url, -1, NULL);
  
  result = sqlite3_step(stmt);
  if (result != SQLITE_DONE) {
    musicd_log(LOG_ERROR, "library", "set_url_mtime: sqlite3_step failed.");
  }
  
  sqlite3_finalize(stmt);
}

int library_open()
{
  int error = 0;
  char *file;
  
  file = config_to_path("db-file");
  if (!file) {
    musicd_log(LOG_ERROR, "library", "db-file not set");
    return -1;
  }
  
  if (sqlite3_open(file, &db) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not open database '%s': %s", file, sqlite3_errmsg(db));
    return -1;
  }
  //id INTEGER PRIMARY KEY ASC,
  simple_exec("CREATE TABLE IF NOT EXISTS urls (url TEXT UNIQUE, mtime INT64)", &error);
  simple_exec("CREATE TABLE IF NOT EXISTS artists (name TEXT UNIQUE)", &error);
  simple_exec("CREATE TABLE IF NOT EXISTS albums (name TEXT UNIQUE, artist INT)", &error);
  simple_exec("CREATE TABLE IF NOT EXISTS tracks (url INT, number INT, name TEXT, artist INT, album INT, start INT, duration INT)", &error);
  
  if (error) {
    musicd_log(LOG_ERROR, "library", "Could not create database tables.");
    return -1;
  }
  
  return 0;
  
}


int library_add(track_t *track)
{
  int url, artist, album;
  sqlite3_stmt *stmt;
  
  /*musicd_log(LOG_DEBUG, "library", "%s %d %s", track->url, track->number, track->name);*/
  
  url = field_id("urls", "url", track->url);
  artist = field_id("artists", "name", track->artist);
  album = field_id("albums", "name", track->album);
  
  static const char *sql =
    "INSERT INTO tracks (url, number, name, artist, album, start, duration) VALUES(?, ?, ?, ?, ?, ?, ?)";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not execute '%s': %s", sql, sqlite3_errmsg(db));
    return -1;
  }
  
  sqlite3_bind_int(stmt, 1, url);
  sqlite3_bind_int(stmt, 2, track->number);
  sqlite3_bind_text(stmt, 3, track->name, -1, NULL);
  sqlite3_bind_int(stmt, 4, artist);
  sqlite3_bind_int(stmt, 5, album);
  sqlite3_bind_int(stmt, 6, track->start);
  sqlite3_bind_int(stmt, 7, track->duration);
  
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    /* Ok, now it really is an error. */
    musicd_log(LOG_ERROR, "library", "library_add: sqlite3_step failed.");
    sqlite3_finalize(stmt);
    return -1;
  }
  
  sqlite3_finalize(stmt);
  return 0;
}

/*struct library_query_t {
  sqlite3_stmt *stmt;
};*/

library_query_t *library_search(const char *search)
{
  sqlite3_stmt *result;
  static const char *sql =
    "SELECT tracks.rowid AS id, urls.url AS url, tracks.number AS number, tracks.name AS name, artists.name AS artist, albums.name AS album, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE (COALESCE(tracks.name, '') || COALESCE(artists.name, '') || COALESCE(albums.name, '')) LIKE ?";
      
  /* WHERE tracks.name LIKE ? OR artists.name LIKE ? OR albums.name LIKE ? */
  /* FIXME: The condition could simply be 'WHERE (tracks.name || artists.name || albums.name) LIKE ?'
   * BUT it does not work for some strange reason - if there is an empty string, concatenation
   * apparently returns NULL or something similar. */
  
  if (sqlite3_prepare_v2(db, sql, -1, &result, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s", sql, sqlite3_errmsg(db));
    return NULL;
  }
  
  sqlite3_bind_text(result, 1, search, -1, NULL);
  /*sqlite3_bind_text(result, 2, search, -1, NULL);
  sqlite3_bind_text(result, 3, search, -1, NULL);*/
  
  return (library_query_t*)result;
}

int library_search_next(library_query_t *query, track_t* track)
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
  track->number = sqlite3_column_int((sqlite3_stmt*)query, 2);
  track->name = (char*)sqlite3_column_text((sqlite3_stmt*)query, 3);
  track->artist = (char*)sqlite3_column_text((sqlite3_stmt*)query, 4);
  track->album = (char*)sqlite3_column_text((sqlite3_stmt*)query, 5);
  track->duration = sqlite3_column_int((sqlite3_stmt*)query, 6);
  return 0;
}

void library_search_close(library_query_t *query)
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
    "SELECT tracks.rowid AS id, urls.url AS url, tracks.number AS number, tracks.name AS name, artists.name AS artist, albums.name AS album, tracks.start AS start, tracks.duration AS duration FROM tracks JOIN urls ON tracks.url = urls.rowid JOIN artists ON tracks.artist = artists.rowid JOIN albums ON tracks.album = albums.rowid WHERE tracks.rowid = ?";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "library", "Could not prepare '%s': %s", sql, sqlite3_errmsg(db));
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
  track->number = sqlite3_column_int(stmt, 2);
  track->name = dup_or_empty((const char*)sqlite3_column_text(stmt, 3));
  track->artist = dup_or_empty((const char*)sqlite3_column_text(stmt, 4));
  track->album = dup_or_empty((const char*)sqlite3_column_text(stmt, 5));
  track->start = sqlite3_column_int(stmt, 6);
  track->duration = sqlite3_column_int(stmt, 7);
  
  musicd_log(LOG_DEBUG, "library", "%i %s %i %s %s %s %i %i", track->id,
             track->url, track->number, track->name, track->artist,
             track->album, track->start, track->duration);
  
  return track;
}


static int interrupted = 0;

static void scan_signal_handler()
{
  interrupted = 1;
}

static void
iterate_dir(const char *path,
            void (*callback)(const char *path, const char *ext))
{
  DIR *dir;
  struct dirent *entry;
  struct stat status;
  char *extension;
  
  /* + 256 4-bit UTF-8 characters + / and \0 
   * More than enough on every platform really in use. */
  char filepath[strlen(path) + 1024 + 2];
  
  if (!(dir = opendir(path))) {
    /* Probably no read access - ok, we just omit. */
    musicd_perror(LOG_WARNING, "library", "Could not open directory %s", path);
    return;
  }
  
  signal(SIGINT, scan_signal_handler);
  
  errno = 0;
  while ((entry = readdir(dir))) {
    if (interrupted) {
      signal(SIGINT, NULL);
      return;
    }
    
    /* Omit hidden files and most importantly . and .. */
    if (entry->d_name[0] == '.') {
      goto next;
    }
    
    sprintf(filepath, "%s/%s", path, entry->d_name);
    extension = entry->d_name + strlen(entry->d_name) - 3;
   
    /* Stat only if this is a symbolic link, as there is no need to resolve
     * anything with stat otherwise. */
    if (entry->d_type == DT_LNK) {
      if (stat(filepath, &status)) {
        goto next;
      }
      
      if (S_ISDIR(status.st_mode)) {
        iterate_dir(filepath, callback);
        
      } else if (S_ISREG(status.st_mode)) {
        callback(filepath, extension);
      }
    } else {
    
      if (entry->d_type == DT_DIR) {
        iterate_dir(filepath, callback);
        goto next;
      }
      
      if (entry->d_type != DT_REG) {
        goto next;
      }
      
      callback(filepath, extension);
    }
    
  next:
    errno = 0;
  }
  
  closedir(dir);
  
  signal(SIGINT, NULL);
  
  if (errno) {
    /* It was possible to open the directory but we can't iterate it anymore?
     * Something's fishy. */
    musicd_perror(LOG_ERROR, "library", "Could not iterate directory %s", path);
  }
}


static void scan_cue_cb(const char *path, const char *ext)
{  
  if (strcasecmp(ext, "cue")) {
    return;
  }
  cue_read(path);
}

static void scan_files_cb(const char *path, const char *ext)
{
  track_t *track;
  struct stat status;
  
  if (!strcasecmp(ext, "cue")
   || !strcasecmp(ext, "jpg")
   || !strcasecmp(ext, "png")
   || !strcasecmp(ext, "gif")
  ) {
    return;
  }
  
  if (stat(path, &status)) {
    return;
  }
  
  if (library_get_url_mtime(path) >= status.st_mtime) {
    return;
  }
  
  library_set_url_mtime(path, status.st_mtime);
  
  musicd_log(LOG_DEBUG, "library", "scan %s", path);
  
  track = track_from_url(path);
  if (!track) {
    return;
  }

  library_add(track);
  track_free(track);
}

static void scan_dir(const char *path)
{
  iterate_dir(path, scan_cue_cb);
  iterate_dir(path, scan_files_cb);
}

void library_scan(const char *raw_path)
{
  char path[strlen(raw_path)];
  strcpy(path, raw_path);
  
  /* Strip possible trailing / */
  if (path[strlen(path) - 1] == '/') {
    path[strlen(path) - 1] = '\0';
  }
  
  musicd_log(LOG_INFO, "library", "Start scan.");
  scan_dir(path);
  
  if (interrupted) {
    musicd_log(LOG_INFO, "library", "Scan interrupted, exiting...");
    exit(0);
  }
  
  musicd_log(LOG_INFO, "library", "Scan finished.");
  
}
