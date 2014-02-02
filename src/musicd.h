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
#ifndef MUSICD_MUSICD_H
#define MUSICD_MUSICD_H

#define MUSICD_VERSION_MAJOR 0
#define MUSICD_VERSION_MINOR 3
#define MUSICD_VERSION_MICRO 0
#define MUSICD_VERSION_TAG -dev


#define MUSICD_STRINGIZE2(s) #s
#define MUSICD_STRINGIZE(s) MUSICD_STRINGIZE2(s)

#define MUSICD_VERSION_STRING \
  MUSICD_STRINGIZE(MUSICD_VERSION_MAJOR) \
  "." \
  MUSICD_STRINGIZE(MUSICD_VERSION_MINOR) \
  "." \
  MUSICD_STRINGIZE(MUSICD_VERSION_MICRO) \
  MUSICD_STRINGIZE(MUSICD_VERSION_TAG)

#include <time.h>

time_t musicd_uptime();

#endif
