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
#include "db.h"

#include "config.h"
#include "log.h"
#include "strings.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sqlite3.h>

static sqlite3 *db;

static char *uid;

static int create_schema();

int db_open()
{
  char *file;

  file = config_to_path("db-file");
  if (!file) {
    musicd_log(LOG_ERROR, "db", "db-file not set");
    return -1;
  }

  if (sqlite3_open(file, &db) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "db", "can't open '%s': %s", file, db_error());
    return -1;
  }
  
  if (create_schema()) {
    musicd_log(LOG_ERROR, "db", "can't create schema");
    musicd_log(LOG_ERROR, "db", "database corrupted, reseting");
    db_close();
    
    remove(file);
    
    if (sqlite3_open(file, &db) != SQLITE_OK) {
      musicd_log(LOG_ERROR, "db", "can't open '%s': %s", file, db_error());
      return -1;
    }
    
    if (create_schema()) {
      musicd_log(LOG_ERROR, "db", "can't create schema after database reset");
      musicd_log(LOG_ERROR, "db", "this can be a bug, please report");
      return -1;
    }
  }
  
  return 0;
}

void db_close()
{
  sqlite3_close(db);
  db = NULL;
}

const char* db_error()
{
  return sqlite3_errmsg(db);
}


sqlite3 *db_handle()
{
  return db;
}


void db_simple_exec(const char *sql, int *error)
{
  int result = sqlite3_exec(db_handle(), sql, NULL, NULL, NULL);
  if (result != SQLITE_OK) {
    musicd_log(LOG_ERROR, "db", "can't execute '%s': %s", sql,
               sqlite3_errmsg(db));
    if (error != NULL) {
      *error = result;
    }
  }
}

const char *db_uid()
{
  return uid;
}


static sqlite3_stmt *meta_get(const char *key)
{
  sqlite3_stmt *stmt;
  int result;
  static const char *sql = "SELECT value FROM musicd WHERE key = ?";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "db", "can't query metadata: %s", db_error());
    return 0;
  }
  
  sqlite3_bind_text(stmt, 1, key, -1, NULL);
  
  result = sqlite3_step(stmt);
  if (result == SQLITE_DONE) {
    sqlite3_finalize(stmt);
    return NULL;
  } else if (result != SQLITE_ROW) {
    musicd_log(LOG_ERROR, "db", "meta_get: sqlite3_step failed");
    sqlite3_finalize(stmt);
    return NULL;
  }
  return stmt;
}
static sqlite3_stmt *meta_set(const char *key)
{
  sqlite3_stmt *stmt;
  static const char *sql = "INSERT OR REPLACE INTO musicd VALUES (?, ?)";
  
  if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
    musicd_log(LOG_ERROR, "db", "can't set metadata: %s", db_error());
    return NULL;
  }

  sqlite3_bind_text(stmt, 1, key, -1, NULL);
  return stmt;
}


int db_meta_get_int(const char *key)
{
  int result;
  sqlite3_stmt *stmt = meta_get(key);

  if (!stmt) {
    return 0;
  }

  result = sqlite3_column_int(stmt, 0);
  sqlite3_finalize(stmt);

  return result;
}
void db_meta_set_int(const char *key, int value)
{
  sqlite3_stmt *stmt = meta_set(key);
  if (!stmt) {
    return;
  }

  sqlite3_bind_text(stmt, 1, key, -1, NULL);
  sqlite3_bind_int(stmt, 2, value);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

char *db_meta_get_string(const char *key)
{
  const char *result;
  sqlite3_stmt *stmt = meta_get(key);

  if (!stmt) {
    return 0;
  }

  result = (const char *)sqlite3_column_text(stmt, 0);
  sqlite3_finalize(stmt);

  return strcopy(result);
}
void db_meta_set_string(const char *key, const char *value)
{
  sqlite3_stmt *stmt = meta_set(key);
  if (!stmt) {
    return;
  }

  sqlite3_bind_text(stmt, 1, key, -1, NULL);
  sqlite3_bind_text(stmt, 2, value, -1, NULL);

  sqlite3_step(stmt);
  sqlite3_finalize(stmt);
}

static void generate_uid()
{
  uid = stringf("%" PRIx64 "%" PRIx64 "", (int64_t)time(NULL), (int64_t)getpid());
}

static int create_schema()
{
  int error = 0;
  int schema;

  db_simple_exec("CREATE TABLE IF NOT EXISTS musicd (key TEXT UNIQUE, value TEXT)", &error);
  if (error) {
    musicd_log(LOG_ERROR, "db", "can't create master table");
    return -1;
  }
  
  schema = db_meta_get_int("schema");
  
  musicd_log(LOG_DEBUG, "db", "schema: %d", schema);

  if (schema > MUSICD_DB_SCHEMA) {
    musicd_log(LOG_ERROR, "db", "schema version higher than supported");
    return -1;
  }

  if (schema < MUSICD_DB_SCHEMA) {
    musicd_log(LOG_INFO, "db", "new database or old schema");

    /* Clear meta table */
    db_simple_exec("DELETE FROM musicd", &error);

    /* Drop all known tables (if exist) */
    db_simple_exec("DROP TABLE IF EXISTS directories", &error);
    db_simple_exec("DROP TABLE IF EXISTS urls", &error);
    db_simple_exec("DROP TABLE IF EXISTS artists", &error);
    db_simple_exec("DROP TABLE IF EXISTS albums", &error);
    db_simple_exec("DROP TABLE IF EXISTS tracks", &error);
    db_simple_exec("DROP TABLE IF EXISTS images", &error);
    db_simple_exec("DROP TABLE IF EXISTS lyrics", &error);
    
    db_simple_exec("CREATE TABLE directories (path TEXT UNIQUE, mtime INT64, parent INT64)", &error);
    db_simple_exec("CREATE TABLE files (path TEXT UNIQUE, mtime INT64, directory INT64)", &error);
    db_simple_exec("CREATE TABLE artists (name TEXT UNIQUE)", &error);
    db_simple_exec("CREATE TABLE albums (name TEXT UNIQUE, artist INT64, image INT64)", &error);
    db_simple_exec("CREATE TABLE tracks (fileid INT64, file TEXT, cuefileid INT64, cuefile TEXT, track INT, title TEXT, artistid INT64, artist TEXT, albumid INT64, album TEXT, start DOUBLE, duration DOUBLE)", &error);
    db_simple_exec("CREATE TABLE images (file INT64, album INT64)", &error);
    db_simple_exec("CREATE TABLE lyrics (track INT64 UNIQUE, lyrics TEXT, provider TEXT, source TEXT, mtime INT64)", &error);

    /* Index for good default sorting */
    db_simple_exec("CREATE INDEX tracks_default_index ON tracks (album COLLATE NOCASE ASC, track COLLATE NOCASE ASC, title COLLATE NOCASE ASC)", &error);

    generate_uid();
    db_meta_set_string("uid", uid);
  }
  
  if (error) {
    musicd_log(LOG_ERROR, "db", "can't create database tables");
    return -1;
  }
  
  db_meta_set_int("schema", MUSICD_DB_SCHEMA);
  
  if (error) {
    musicd_log(LOG_ERROR, "db", "can't create schema");
    return -1;
  }
  
  return 0;
}

