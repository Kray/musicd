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
#ifndef MUSICD_TRACK_H
#define MUSICD_TRACK_H

#include "libav.h"

typedef struct track {
  int64_t id;
  
  char *path;
  
  int track;
  char *title;
  int64_t artistid;
  char *artist;
  int64_t albumid;
  char *album;
  char *albumartist;
  
  int duration;
  
  int start;
  
} track_t;


track_t *track_new();

track_t *track_from_path(const char *path);

void track_free(track_t *track);


#endif
