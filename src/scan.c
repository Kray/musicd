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

static bool thread_running = false;
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


static void scan_directory(const char *dirpath, int parent);


static int64_t scan_file(const char *path, int64_t directory)
{
  const char *extension;
  int64_t url = 0;
  track_t *track;

  for (extension = path + strlen(path);
    *(extension) != '.' && extension != path; --extension) { }
  ++extension;
    
  if (!strcasecmp(extension, "jpg")
    || !strcasecmp(extension, "png")
    || !strcasecmp(extension, "gif")
  ) {
    /* image */
  } else if (!strcasecmp(extension, "cue")) {
    if (cue_read(path, directory)) {
      url = library_url(path, directory);
    }
  } else {
    musicd_log(LOG_DEBUG, "scan", "File: %s %s", extension, path);
    track = track_from_path(path);
    if (track) {
      url = library_url(path, directory);
      if (url <= 0) {
        musicd_log(LOG_ERROR, "scan", "Could not create url into database?");
      } else {
        library_track_add(track, url);
      }
      track_free(track);
    }
  } 
  return url;
}

/**
 * Iterates through directory. Subdirectories will be scanned using
 * scan_directory.
 */
static void iterate_directory(const char *dirpath, int dir_id)
{
  struct stat status;
  DIR *dir;
  struct dirent *entry;
  
  int64_t url;
  time_t url_mtime;
  
  /* + 256 4-bit UTF-8 characters + / and \0 
   * More than enough on every platform really in use. */
  char *path = malloc(strlen(dirpath) + 1024 + 2);
  
  if (!(dir = opendir(dirpath))) {
    /* Probably no read access - ok, we just omit. */
    musicd_perror(LOG_WARNING, "scan", "Could not open directory %s", path);
    return;
  }
  
  errno = 0;
  while ((entry = readdir(dir))) {
    if (interrupted) {
      break;
    }
    
    /* Omit hidden files and most importantly . and .. */
    if (entry->d_name[0] == '.') {
      goto next;
    }
    
    sprintf(path, "%s/%s", dirpath, entry->d_name);

    if (stat(path, &status)) {
      goto next;
    }
      
    if (S_ISDIR(status.st_mode)) {
      scan_directory(path, dir_id);
      goto next;
    }
    if (!S_ISREG(status.st_mode)) {
      goto next;
    }
    
    url = library_url(path, 0);
    if (url > 0) {
      url_mtime = library_url_mtime(url);
      if (url_mtime == status.st_mtime) {
        goto next;
      }
    }
    
    url = scan_file(path, dir_id);
    if (url) {
      library_url_mtime_set(url, status.st_mtime);
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


static bool scan_urls_cb(library_url_t *url)
{
  struct stat status;
  
  if (stat(url->path, &status)) {
    musicd_perror(LOG_DEBUG, "scan", "Removing url %s",
                  url->path);
    library_url_delete(url->id);
    return true;
  }
  
  if (url->mtime == status.st_mtime) {
    return true;
  }
  
  library_url_clear(url->id);
  if (scan_file(url->path, url->directory)) {
    library_url_mtime_set(url->id, status.st_mtime);
  } else {
    library_url_delete(url->id);
  }
  
  return true;
}


static bool scan_directory_cb(library_directory_t *directory)
{
  struct stat status;
  
  if (stat(directory->path, &status)) {
    musicd_perror(LOG_WARNING, "scan", "Could not stat directory %s",
                  directory->path);
    return true;
  }

  if (interrupted) {
    return false;
  }
  
  if (directory->mtime == status.st_mtime) {
    library_iterate_directories(directory->id, scan_directory_cb);
    return true;
  }
  
  library_iterate_urls_by_directory(directory->id, scan_urls_cb);
  iterate_directory(directory->path, directory->id);
  
  if (interrupted) {
    return false;
  }

  library_directory_mtime_set(directory->id, status.st_mtime);
  
  return true;
}

static void scan_directory(const char *dirpath, int parent)
{
  int dir_id;
  time_t dir_mtime;
  struct stat status;
  
  if (stat(dirpath, &status)) {
    musicd_perror(LOG_WARNING, "scan", "Could not stat directory %s", dirpath);
    return;
  }
  
  dir_id = library_directory(dirpath, parent);
  dir_mtime = library_directory_mtime(dir_id);
  
  if (dir_mtime == status.st_mtime) {
    library_iterate_directories(dir_id, scan_directory_cb);
    return;
  }
  
  library_iterate_urls_by_directory(dir_id, scan_urls_cb);
  iterate_directory(dirpath, dir_id);
  
  if (interrupted) {
    return;
  }
  
  library_directory_mtime_set(dir_id, status.st_mtime);
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
  
  musicd_log(LOG_INFO, "scan", "Starting scanning.");
  
  signal(SIGINT, scan_signal_handler);
  
  scan_directory(path, 0);
  
  free(path);
  
  signal(SIGINT, NULL);
  
  if (interrupted) {  
    musicd_log(LOG_INFO, "scan", "Scanning interrupted.");
    return;
  }
  
  musicd_log(LOG_INFO, "scan", "Scanning finished.");
  
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