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

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <libgen.h>

static int read_line(FILE *file, char *buf, int buf_size) {
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
 * @returns String length
 */
static int read_string(const char *src, char *dst)
{
  const char *p = src;
  int pos = 0;
  
  if (*p == '"') {
    ++p;
  }
  
  do {
    if (p == '\0') {
      break;
    }
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
  } while ((*p != '"' && p != '\0'));
  
  dst[pos++] = '\0';
  
  return p - src;
}

/**
 * @todo FIXME Multiple files in same cue sheet
 * @todo FIXME Major cleanup - full rewrite probably not necessary.
 */
void cue_read(const char* path)
{
  FILE *file;
  char base_path[strlen(path)];
  time_t mtime, cue_mtime;
  struct stat status;
  char line[1024], instr[16], string1[512], *ptr;
  char url[strlen(path) + 1024 + 2], cueurl[strlen(path) + 1024 + 2];
  char album[512], albumartist[512];
  int i, header_read = 0;
  int index, mins, secs, frames;
  /* Track is stored in prev_track until index of the following track is known.
   * This is mandatory for figuring out the track's duration. Last track's
   * duration is calculated from file's total duration. */
  track_t *prev_track = NULL, *track = NULL, *file_track = NULL;
  
  /* Extract base path. */
  for (i = strlen(path) - 1; i > 0 && path[i] != '/'; --i) { }
  strncpy(base_path, path, i);
  base_path[i] = '\0';
  
  if (stat(path, &status)) {
    return;
  }
  
  /* Modification timestamp of the cue sheet file. */
  cue_mtime = status.st_mtime;
  
  url[0] = '\0';
  album[0] = '\0';
  albumartist[0] = '\0';
  
  file = fopen(path, "r");
  if (!file) {
    musicd_log(LOG_ERROR, "cue", "Could not open file %s", path);
    return;
  }
  
  /* Check for BOM */
  fread(line, 1, 3, file);
  if (line[0] == (char)0xef && line[1] == (char)0xbb && line[2] == (char)0xbf) {
    /*musicd_log(LOG_DEBUG, "cue", "BOM detected.");*/
  } else {
    fseek(file, 0, SEEK_SET);
  }

  while (read_line(file, line, sizeof(line))) {
    if (sscanf(line, "%15s", instr) < 1) {
      continue;
    }
    
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
        track->artist = strdup(string1);
      }
    }
    if (!strcmp(instr, "TITLE")) {
      /*musicd_log(LOG_DEBUG, "cue", "name: %s", string1);*/
      if (!header_read) {
        strcpy(album, string1);
      } else if (track) {
        free(track->title);
        track->title = strdup(string1);
      }
    }
    if (!strcmp(instr, "FILE")) {
      if (file_track) {
        musicd_log(LOG_WARNING, "cue", "Multiple FILEs in a single cue sheet "
                                       "is currently unsupported. Sorry.");
        break;
      }
      
      header_read = 1;
      
      sprintf(url, "%s/%s", base_path, string1);
      
      if (stat(url, &status)) {
        break;
      }
      
      mtime = library_get_url_mtime(url);
      
      if (cue_mtime <= mtime && status.st_mtime <= mtime) {
        /* Neither cue sheet nor the file itself has newer timestamp, skip. */
        break;
      }
      
      musicd_log(LOG_DEBUG, "cue", "cue sheet: %s", path);
      
      /* Set the highest corresponding timestamp in database. This way the url
       * won't be rescanned when looking for ordinary track files.
       */
      library_set_url_mtime(url, cue_mtime > status.st_mtime ?
                                 cue_mtime : status.st_mtime);
      
      /* Prioritizing: if there are multiple cue sheets and a cue sheet with
       * same base name as the track file exists, it is used for the track.
       * otherwise, sheet with highest mtime will result to be selected.
       */
      for (i = strlen(url) - 1; i > 0 && url[i] != '.'; --i) { }
      strncpy(cueurl, url, i);
      strcpy(cueurl + i, ".cue");
      
      if (strcmp(path, cueurl) && stat(cueurl, &status) == 0) {
        musicd_log(LOG_DEBUG, "cue",
                   "Multiple cue sheets for '%s', selecting '%s'",
                   url, cueurl);
        cue_read(cueurl);
        break;
      }
      
      file_track = track_from_url(url);
      if (!file_track) {
        break;
      }
      
      library_clear_url(url);
     
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
          library_add(prev_track);
          track_free(prev_track);
          prev_track = track;
        }
      }
      
      track = track_new();
      track->url = strdup(url);
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
        track->start = mins * 60 + secs;
      }
    }    
    
  }
  
  if (prev_track) {
    prev_track->duration = track->start - prev_track->start;
    library_add(prev_track);
    track_free(prev_track);
  }
  if (track) {
    track->duration = file_track->duration - track->start;
    library_add(track);
    track_free(track);
  }
  
  track_free(file_track);
  
  fclose(file);
  
}
