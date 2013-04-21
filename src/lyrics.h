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
#ifndef MUSICD_LYRICS_H
#define MUSICD_LYRICS_H

#include "track.h"

typedef struct lyrics {
  char *lyrics;
  char *source;
} lyrics_t;

lyrics_t *lyrics_new();
void lyrics_free();

/**
 * Lyrics fetching (some more or less ugly scraping and magic) from
 * lyrics.wikia.com
 */
lyrics_t *lyrics_fetch(const track_t *track);

/**
 * Lyrics fetching task handler.
 * @p id_ptr int64_t * which lyrics_task will free itself
 */
void *lyrics_task(void *id_ptr);

#endif
