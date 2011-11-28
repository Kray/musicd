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
#include "scan.h"

#include "config.h"
#include "cue.h"
#include "db.h"
#include "library.h"
#include "log.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

static volatile bool thread_running = false;
static pthread_t thread;

static time_t last_scan = 0;

static void *scan_thread_func(void *data);

int scan_start()
{
  if (thread_running) {
    return 0;
  }
  
  if (pthread_create(&thread, NULL, scan_thread_func, NULL)) {
    musicd_perror(LOG_ERROR, "scan", "Could not create thread");
    return -1;
  }
  
  thread_running = true;
  
  return 0;
}


static int interrupted = 0;

static void scan_signal_handler()
{
  interrupted = 1;
}

/**
 * Stats an url - if can't stat or not regular file, remove the url and
 * associated tracks.
 * @todo FIXME Also handle possible erased artists, albums, and so on.
 */
static bool check_url(const char *url)
{
  struct stat status;

  if (interrupted) {
    return false;
  }
  
  if (!url) {
    /* What? This shouldn't happen. Clear the url anyway. */
    musicd_log(LOG_WARNING, "scan",
                "Empty url in database. This is probably caused by a bug.");
  } else {
    if (!stat(url, &status)) {
      if (S_ISREG(status.st_mode)) {
        /* Regular file. Nothing to see here. */
        return true;
      }
    }
  }
  
  /* We didn't get url, file can't be statted or it isn't a regular file.
   * Get rid of it and tracks associated with it. */
  musicd_log(LOG_DEBUG, "scan", "Remove entries for removed url '%s'",
              url);
  library_clear_url(url);
  library_delete_url(url);
  
  return true;
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
    musicd_perror(LOG_WARNING, "scan", "Could not open directory %s", path);
    return;
  }
  
  errno = 0;
  while ((entry = readdir(dir))) {
    if (interrupted) {
      return;
    }
    
    /* Omit hidden files and most importantly . and .. */
    if (entry->d_name[0] == '.') {
      goto next;
    }
    
    sprintf(filepath, "%s/%s", path, entry->d_name);
    extension = entry->d_name + strlen(entry->d_name) - 3;
   

    /* Stat only if file type is not available or this is a symbolic link, as
     * there is no need to resolve anything with stat otherwise. */
    if (entry->d_type == 0 || entry->d_type == DT_LNK) {
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
  
  if (errno) {
    /* It was possible to open the directory but we can't iterate it anymore?
     * Something's fishy. */
    musicd_perror(LOG_ERROR, "scan", "Could not iterate directory %s",
                  path);
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
  
  library_clear_url(path);
  
  musicd_log(LOG_DEBUG, "library", "scan %s", path);
  
  musicd_log(LOG_DEBUG, "library", "%i", av_guess_format(NULL, path, NULL));
  
  track = track_from_url(path);
  if (!track) {
    return;
  }

  library_set_url_mtime(path, status.st_mtime);

  library_add(track);
  track_free(track);
}

static void scan_dir(const char *path)
{
  iterate_dir(path, scan_cue_cb);
  iterate_dir(path, scan_files_cb);
}

static void scan() {
  const char *raw_path = config_to_path("music-directory");
  char *path;
  time_t now;
  
  if (raw_path == NULL) {
    musicd_log(LOG_INFO, "scan", "music-directory not set, not scanning.");
    return;
  }
  
  path = strdup(raw_path);
  
  /* Strip possible trailing / */
  if (path[strlen(path) - 1] == '/') {
    path[strlen(path) - 1] = '\0';
  }
  
  last_scan = db_meta_get_int("last-scan");
  now = time(NULL);
  
  musicd_log(LOG_INFO, "scan", "Start scan.");
  
  signal(SIGINT, scan_signal_handler);
  
  library_iterate_urls(check_url);
  scan_dir(path);
  
  signal(SIGINT, NULL);
  
  if (interrupted) {  
    musicd_log(LOG_INFO, "scan", "Scan interrupted.");
    return;
  }
  
  musicd_log(LOG_INFO, "scan", "Scan finished.");
  
  free(path);
  
  db_meta_set_int("last-scan", now);
}

static void *scan_thread_func(void *data)
{
  (void)data;
  
  pthread_detach(pthread_self());
  
  db_simple_exec("BEGIN TRANSACTION", NULL);
  scan();
  db_simple_exec("COMMIT TRANSACTION", NULL);

  thread_running = false;
  
  /** @todo FIXME */
  if (interrupted) {  
    exit(0);
  }
  
  return NULL;
}