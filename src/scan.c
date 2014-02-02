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
#include "strings.h"

#include <dirent.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pthread.h>

#include <FreeImage.h>


static bool thread_running = false;

static pthread_t scan_thread;
static pthread_mutex_t scan_mutex = PTHREAD_MUTEX_INITIALIZER;

static time_t last_scan = 0;

static scan_status_t status = { };


/**
 * Preferred prefixes for album image file names.
 * @see scan_image_prefix_changed
 */
static char **image_prefixes = NULL;


static int interrupted = 0, restart = 0;

static void scan_signal_handler()
{
  interrupted = 1;
}

static void *scan_thread_func(void *data);

int scan_start()
{
  pthread_mutex_lock(&scan_mutex);
  if (thread_running) {
    /* Signal the scan thread to restart scanning */
    musicd_log(LOG_VERBOSE, "scan", "signaling to restart scan");
    restart = 1;
    interrupted = 1;
    pthread_mutex_unlock(&scan_mutex);
    return 0;
  }
  pthread_mutex_unlock(&scan_mutex);

  if (!config_get_value("music-directory")) {
    musicd_log(LOG_WARNING, "scan", "music-directory not set, no scanning");
    return 0;
  }

  thread_running = true;

  if (pthread_create(&scan_thread, NULL, scan_thread_func, NULL)) {
    musicd_perror(LOG_ERROR, "scan", "could not create thread");
    return -1;
  }
  pthread_detach(scan_thread);
  
  return 0;
}

void scan_track_added()
{
  pthread_mutex_lock(&scan_mutex);
  ++status.new_tracks;
  pthread_mutex_unlock(&scan_mutex);
}

void scan_status(scan_status_t *status_out)
{
  pthread_mutex_lock(&scan_mutex);
  memcpy(status_out, &status, sizeof(scan_status_t));
  pthread_mutex_unlock(&scan_mutex);
}


static void scan_directory(const char *dirpath, int parent);

static int64_t scan_file(const char *path, int64_t directory)
{
  const char *extension;
  int64_t file = 0;
  track_t *track;

  for (extension = path + strlen(path);
    *(extension) != '.' && extension != path; --extension) { }
  ++extension;
    
  if (!strcasecmp(extension, "cue")) {
    /* CUE sheet */
    musicd_log(LOG_DEBUG, "scan", "cue: %s", path);
    cue_read(path, directory);
  } else if(FreeImage_GetFIFFromFilename(path) != FIF_UNKNOWN) {
    /* Image file */
    if (FreeImage_GetFileType(path, 0) != FIF_UNKNOWN) {
      musicd_log(LOG_DEBUG, "scan", "image: %s", path);
      file = library_file(path, directory);
      library_image_add(file);
    }
  } else {
    /* Try track */
    track = track_from_path(path);
    if (track) {
      musicd_log(LOG_DEBUG, "scan", "track: %s", path);
      library_track_add(track, directory);
      scan_track_added();
      track_free(track);
      file = library_file(path, 0);
    }
  } 
  return file;
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
  
  int64_t file;
  time_t file_mtime;
  
  char *path;
  
  if (!(dir = opendir(dirpath))) {
    /* Probably no read access - ok, we just omit. */
    musicd_perror(LOG_WARNING, "scan", "could not open directory %s", dirpath);
    return;
  }
  
  /* + 256 4-bit UTF-8 characters + / and \0 
   * More than enough on every platform really in use. */
  path = malloc(strlen(dirpath) + 1024 + 2);
  
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
    
    file = library_file(path, 0);
    if (file > 0) {
      file_mtime = library_file_mtime(file);
      if (file_mtime == status.st_mtime) {
        goto next;
      }
    }
    
    file = scan_file(path, dir_id);
    if (file) {
      library_file_mtime_set(file, status.st_mtime);
    }

  next:
    errno = 0;
  }
  
  closedir(dir);
  
  if (errno) {
    /* It was possible to open the directory but we can't iterate it anymore?
     * Something's fishy. */
    musicd_perror(LOG_ERROR, "scan", "could not iterate directory %s", path);
  }
  free(path);
}


static bool scan_files_cb(library_file_t *file)
{
  struct stat status;
  
  if (stat(file->path, &status)) {
    musicd_perror(LOG_DEBUG, "scan", "removing file %s", file->path);
    library_file_delete(file->id);
    return true;
  }
  
  if (file->mtime == status.st_mtime) {
    return true;
  }
  
  library_file_clear(file->id);
  if (scan_file(file->path, file->directory)) {
    library_file_mtime_set(file->id, status.st_mtime);
  } else {
    library_file_delete(file->id);
  }
  
  return true;
}

struct albumimg_comparison {
  int64_t id;
  char *name;
  int level;
};

static bool update_albumimg_cb(library_image_t *image, void *opaque)
{
  char *name;
  int level, diff;
  bool better = false;
  struct albumimg_comparison *comparison = opaque;

  /* Extract what's between last '/' and last '.' in the path */

  const char *p1 = image->path + strlen(image->path), *p2 = NULL;

  for (; p1 > image->path && *(p1 - 1) != '/'; --p1) {
    if (*p1 == '.' && !p2) {
      p2 = p1;
    }
  }
  if (!p2) {
    p2 = image->path + strlen(image->path);
  }

  name = strextract(p1, p2);
  if (image_prefixes) {
    for (level = 0; image_prefixes[level]; ++level) {
      if (!strncasecmp(image_prefixes[level], name, strlen(image_prefixes[level]))) {
        /* Matches current level */
        break;
      }
    }
  } else {
    level = 0;
  }

  /* First entry */
  if (!comparison->id) {
    better = true;
    goto skip;
  }

  /* Lower level than previous best, must be better */
  if (level < comparison->level) {
    better = true;
    goto skip;
  }

  /* Higher level than previous best, can't be better */
  if (level > comparison->level) {
    goto skip;
  }

  diff = strcasecmp(name, comparison->name);
  if (diff < 0) {
    /* "Smaller" means better */
    better = true;
  }

skip:
  if (better) {
    comparison->id = image->id;
    free(comparison->name);
    comparison->name = name;
    comparison->level = level;
  } else {
    free(name);
  }

  return true;
}

/**
 * Tries to find the best image for @p album based on prefixes in image-prefix.
 */
static void update_albumimg(int64_t album)
{
  struct albumimg_comparison comparison = {
    0, NULL, INT_MAX
  };

  library_iterate_images_by_album(album, update_albumimg_cb, &comparison);

  if (comparison.id > 0) {
    library_album_image_set(album, comparison.id);
  }
}

static bool assign_images_cb(library_directory_t *directory, void *album)
{
  if (!library_directory_tracks_count(directory->id) > 0) {
    library_image_album_set_by_directory(directory->id, *((int64_t *)album));
  }
  return true;
}

static void assign_images(int64_t directory)
{
  int64_t album;
  album = library_album_by_directory(directory);
  if (album <= 0) {
    return;
  }

  library_image_album_set_by_directory(directory, album);

  library_iterate_directories(directory, assign_images_cb, (void *)&album);

  update_albumimg(album);
}

static bool scan_directory_cb(library_directory_t *directory, void *empty)
{
  (void)empty;
  struct stat status;
  if (stat(directory->path, &status)) {
    musicd_perror(LOG_DEBUG, "scan", "removing directory %s", directory->path);
    library_directory_delete(directory->id);
    return true;
  }

  if (interrupted) {
    return false;
  }
  
  library_iterate_directories(directory->id, scan_directory_cb, NULL);
  
  if (directory->mtime == status.st_mtime) {
    return true;
  }
  
  library_iterate_files_by_directory(directory->id, scan_files_cb);
  iterate_directory(directory->path, directory->id);
  
  assign_images(directory->id);
  
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
  
  dir_id = library_directory(dirpath, -1);
  
  if (stat(dirpath, &status)) {
    musicd_perror(LOG_WARNING, "scan", "could not stat directory %s", dirpath);
    if (dir_id) {
      library_directory_delete(dir_id);
    }
    return;
  }
  
  if (dir_id > 0) {
    library_iterate_directories(dir_id, scan_directory_cb, NULL);
    dir_mtime = library_directory_mtime(dir_id);
    if (dir_mtime == status.st_mtime) {
      return;
    }
  } else {
    dir_id = library_directory(dirpath, parent);
  }
  
  library_iterate_files_by_directory(dir_id, scan_files_cb);
  iterate_directory(dirpath, dir_id);
  
  assign_images(dir_id);
  
  if (interrupted) {
    return;
  }
  
  library_directory_mtime_set(dir_id, status.st_mtime);
}

static void scan()
{
  const char *raw_path = config_to_path("music-directory");
  char *path;
  time_t now;
  
  if (raw_path == NULL) {
    musicd_log(LOG_INFO, "scan", "music-directory not set, not scanning");
    return;
  }
  
  path = strdup(raw_path);
  
  /* Strip possible trailing / */
  if (path[strlen(path) - 1] == '/') {
    path[strlen(path) - 1] = '\0';
  }
  
  
  last_scan = db_meta_get_int("last-scan");
  now = time(NULL);
  
  musicd_log(LOG_INFO, "scan", "starting");
  
  signal(SIGINT, scan_signal_handler);
  
  scan_directory(path, 0);
  
  free(path);
  
  signal(SIGINT, NULL);
  
  if (interrupted) {  
    musicd_log(LOG_INFO, "scan", "interrupted");
    return;
  }
  
  musicd_log(LOG_INFO, "scan", "finished");
  
  db_meta_set_int("last-scan", now);
}

static void *scan_thread_func(void *data)
{
  (void)data;
  
  pthread_mutex_lock(&scan_mutex);
  memset(&status, 0, sizeof(scan_status_t));
  status.active = true;
  status.start_time = time(NULL);
  pthread_mutex_unlock(&scan_mutex);

  db_simple_exec("BEGIN TRANSACTION", NULL);
  scan();
  db_simple_exec("COMMIT TRANSACTION", NULL);

  pthread_mutex_lock(&scan_mutex);
  thread_running = false;

  status.active = false;
  status.end_time = time(NULL);

  if (restart) {
    restart = 0;
    interrupted = 0;
    pthread_mutex_unlock(&scan_mutex);
    scan_start();
    return NULL;
  }
  pthread_mutex_unlock(&scan_mutex);

  /** @todo FIXME */
  if (interrupted) {
    exit(0);
  }
  
  return NULL;
}

/**
 * Splits @p prefix to an array of strings in @var image_prefixes
 */
void scan_image_prefix_changed(char *prefix)
{
  int count = 1;
  char *p, **p2;

  if (image_prefixes) {
    for (p2 = image_prefixes; *p2 != NULL; ++p2) {
      free(*p2);
    }
    free(image_prefixes);
  }

  if (!prefix) {
    return;
  }

  for (p = prefix; *p != '\0'; ++p) {
    if (*p == ',') {
      ++count;
    }
  }

  image_prefixes = p2 = malloc((count + 1) * sizeof(char *));

  for (p = prefix + 1; *(p - 1) != '\0'; ++p) {
    if (*p == ',' || *p == '\0') {
      *(p2++) = strextract(prefix, p);

      prefix = p + 1;
    }
  }

  *p2 = NULL;
}
