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
#include "cue.h"
#include "library.h"
#include "log.h"
#include "scan.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

static int read_line(FILE *file, char *buf, int buf_size)
{
  int c, begun = 0, pos = 0;
  
  while (1) {
    c = fgetc(file);
    if (c == EOF || c == '\n') {
      break;
    }
    if (c == '\r') {
      continue;
    }
    if (!begun) {
      if (c == ' ' || c == '\t') {
        continue;
      } else {
        begun = 1;
      }
    }
    buf[pos++] = c;
    if (pos == buf_size - 1) {
      break;
    }
  }
  
  buf[pos] = '\0';
  return pos;
}

/**
 * Parses a quote-surrounded string and replaces escape codes \" and \\ .
 * @param src Source string
 * @param dst Destination buffer. Should be at least strlen(src) * 2 + 1
 * @returns Bytes read
 */
static int read_string(const char *src, char *dst)
{
  const char *p = src;
  int pos = 0;
  
  if (*p == '"') {
    ++p;
  }
  
  while (*p != '"' && p != '\0') {
    if (*p == '\\') {
      p++;
      if (*p == '"') {
	dst[pos++] = '"';
      } else {
	dst[pos++] = '\\';
	dst[pos++] = *p++;
      }
    } else {
      dst[pos++] = *p++;
    }
  } 
  
  dst[pos] = '\0';
  
  return p - src;
}

/**
 * @todo FIXME Multiple files in same cue sheet
 * @todo FIXME Rewrite this garbage
 */
bool cue_read(const char *cuepath, int64_t directory)
{
  bool result = true;
  
  FILE *file;
  char *directory_path, *path, *path2;
  
  bool header_read = false;
  
  char album[512], albumartist[512];
  char line[1024], instr[16], string1[512], *ptr;
  
  int64_t track_file;
  
  struct stat status;
  
  //char file[strlen(path) + 1024 + 2], cuefile[strlen(path) + 1024 + 2];
  
  int i;
  
  int index, mins, secs, frames;
  /* Track is stored in prev_track until index of the following track is known.
   * This is mandatory for figuring out the track's length. Last track's
   * length is calculated from file's total length. */
  track_t *prev_track = NULL, *track = NULL, *file_track = NULL;
  
  file = fopen(cuepath, "r");
  if (!file) {
    musicd_perror(LOG_ERROR, "cue", "can't open file %s", cuepath);
    return false;
  }
  
  directory_path = malloc(strlen(cuepath));
  
  /* Extract directory path. */
  for (i = strlen(cuepath) - 1; i > 0 && cuepath[i] != '/'; --i) { }
  strncpy(directory_path, cuepath, i);
  directory_path[i] = '\0';
  
  /* Directory + 256 4-byte UTF-8 characters + '/' + '\0', more than needed. */
  path = malloc(strlen(directory_path) + 1024 + 2);
  path2 = malloc(strlen(directory_path) + 1024 + 2);
  
  album[0] = '\0';
  albumartist[0] = '\0';
  
  /* Check for BOM, seek back if not found. */
  fread(line, 1, 3, file);
  if (line[0] != (char)0xef
   || line[1] != (char)0xbb
   || line[2] != (char)0xbf) {
    fseek(file, 0, SEEK_SET);
  }

  while (read_line(file, line, sizeof(line))) {
    /* Read instruction, up to 15 characters. */
    if (sscanf(line, "%15s", instr) < 1) {
      continue;
    }
    
    
    /* Skip comments. */
    if (!strcmp(instr, "REM")) {
      continue;
    }
    
    ptr = line + strlen(instr) + 1;
    if (ptr[0] == '"') {
      ptr += read_string(ptr, string1) + 1;
    }
    
    if (!strcmp(instr, "PERFORMER")) {
      /*musicd_log(LOG_DEBUG, "cue", "performer: %s", string1);*/
      if (!header_read) {
        strcpy(albumartist, string1);
      } else if (track) {
        free(track->artist);
        track->artist = strdup(string1);
      }
    } else if (!strcmp(instr, "TITLE")) {
      /*musicd_log(LOG_DEBUG, "cue", "title: %s", string1);*/
      if (!header_read) {
        strcpy(album, string1);
      } else if (track) {
        free(track->title);
        track->title = strdup(string1);
      }
    } else if (!strcmp(instr, "FILE")) {
      if (file_track) {
        musicd_log(LOG_WARNING, "cue", "multiple FILEs in a single cue sheet"
                                       "(%s) is currently unsupported, sorry",
                                        cuepath);
        break;
      }
      
      header_read = true;
      
      sprintf(path, "%s/%s", directory_path, string1);
      
      if (stat(path, &status)) {
        result = false;
        break;
      }
      
      /* Prioritizing: if there are multiple cue sheets and a cue sheet with
       * same base name as the track file exists, it is used for the track.
       * otherwise, sheet with highest mtime will result to be selected.
       */
      for (i = strlen(path) - 1; i > 0 && path[i] != '.'; --i) { }
      strncpy(path2, path, i);
      strcpy(path2 + i, ".cue");
      
      if (strcmp(path2, cuepath) && stat(path2, &status) == 0) {
        musicd_log(LOG_DEBUG, "cue",
                   "multiple cue sheets for '%s', trying '%s'",
                   path, path2);
        if (cue_read(path2, directory)) {
          break;
        }
      }
      
      file_track = track_from_path(path);
      if (!file_track) {
        break;
      }
      
      track_file = library_file(path, 0);
      if (track_file > 0) {
        /* File already exists, clear associated tracks. */
        library_file_clear(track_file);
      } else {
        track_file = library_file(path, directory);
        if (track_file <= 0) {
          /* Some error... */
          break;
        }
      }
      
      library_file_mtime_set(track_file, status.st_mtime);
      
      musicd_log(LOG_DEBUG, "cue", "audio: %s", path);
      continue;
    }
    
    
    if (!file_track) {
      continue;
    }
    
    if (!strcmp(instr, "TRACK")) {
      sscanf(ptr, "%d %s", &index, string1);
      /*musicd_log(LOG_DEBUG, "cue", "track: %d '%s'", index, string1);*/
      if (track) {
        if (!prev_track) {
          prev_track = track;
        } else {
          prev_track->duration = track->start - prev_track->start;
          library_track_add(prev_track, directory);
          scan_track_added();
          track_free(prev_track);
          prev_track = track;
        }
      }
      
      track = track_new();
      track->cuefile = strdup(cuepath);
      track->file = strdup(path);
      track->track = index;
      /* Set artist same as the album artist and replace if track spefific
       * artist is later defined. */
      track->artist = strdup(albumartist);
      track->album = strdup(album);
      track->albumartist = strdup(albumartist);
    }
    
    if (!strcmp(instr, "INDEX")) {
      sscanf(ptr, "%d %d:%d:%d", &index, &mins, &secs, &frames);
      /*musicd_log(LOG_DEBUG, "cue", "index: %d %2.2d:%2.2d.%2.2d", index, mins, secs, frames);*/
      if (index == 1) {
        /* One frame is 1/75 seconds */
        track->start = mins * 60.0 + secs + frames / 75.0;
      }
    }    
    
  }
  
  if (prev_track) {
    prev_track->duration = track->start - prev_track->start;
    library_track_add(prev_track, directory);
    scan_track_added();
    track_free(prev_track);
  }
  if (track) {
    track->duration = file_track->duration - track->start;
    library_track_add(track, directory);
    scan_track_added();
    track_free(track);
  }
  
  track_free(file_track);
  
  fclose(file);
  
  free(directory_path);
  
  free(path);
  free(path2);
  
  return result;
}
