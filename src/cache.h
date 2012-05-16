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
#ifndef MUSICD_CACHE_H
#define MUSICD_CACHE_H

#include <stdbool.h>

/**
 * Ensures cache-dir exists.
 */
int cache_open();

bool cache_exists(const char *name);

/**
 * @Returns data @p name from cache or NULL if doesn't exist.
 */
char *cache_get(const char *name, int *size);

void cache_set(const char *name, const char *data, int size);


#endif
