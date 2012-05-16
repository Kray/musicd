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
#include "cache.h"

#include "config.h"
#include "log.h"
#include "strings.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>


static char *build_path(const char *name)
{
  const char *directory = config_to_path("cache-dir");
  return stringf("%s/%s", directory, name);
}

int cache_open()
{
  const char *directory = config_to_path("cache-dir");
  struct stat status;
  if (stat(directory, &status)) {            
    if (mkdir(directory, 0777)) {
      musicd_perror(LOG_ERROR, "cache", "could not create directory %s",
                    directory);
      return -1;
    }
  }
  return 0;
}

bool cache_exists(const char* name)
{
  char *path;
  struct stat status;

  path = build_path(name);
  
  if (stat(path, &status)) {
    free(path);
    return false;
  }
  free(path);
  return true;
}


char *cache_get(const char *name, int *size)
{
  char *path;
  FILE *file;
  char *data;
  
  if (!cache_exists(name)) {
    return NULL;
  }
  
  path = build_path(name);
  
  file = fopen(path, "rb");
  free(path);
  if (!file) {
    /* What? */
    return NULL;
  }
  
  fseek(file, 0, SEEK_END);
  *size = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  data = malloc(*size);
  *size = fread(data, 1, *size, file);
  
  fclose(file);
  
  return data;
}

void cache_set(const char *name, const char *data, int size)
{
  char *path;
  FILE *file;
  
  path = build_path(name);
  
  file = fopen(path, "wb");
  free(path);
  if (!file) {
    /* What? */
    return;
  }
  
  fwrite(data, 1, size, file);
  
  fclose(file);
}


