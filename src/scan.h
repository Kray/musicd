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
#ifndef MUSICD_SCAN_H
#define MUSICD_SCAN_H

#include <stdbool.h>
#include <time.h>

/**
 * Starts scanning thread or does nothing if already active.
 * @returns 0 if already running or successfully started, nonzero on failure.
 */
int scan_start();

void scan_stop();

/**
 * Increments status' new track counter
 */
void scan_track_added();

typedef struct scan_status {
  bool active;

  time_t start_time;
  time_t end_time;

  int new_tracks;
} scan_status_t;

void scan_status(scan_status_t *status);


void scan_image_prefix_changed(char *prefix);

#endif
