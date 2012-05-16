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

#include "image.h"

#include "cache.h"
#include "library.h"
#include "log.h"
#include "strings.h"

#include <math.h>
#include <stdlib.h>

#include <FreeImage.h>

char *image_cache_name(const char* name, int size)
{ 
  /* Round to closest power of two */
  size = pow(2, ceil(log(size)/log(2)));
  return stringf("%s_%d.jpg", name, size);
}


char *image_create_thumbnail(const char *path, int size, int *data_size)
{
  FIBITMAP *fromimg, *toimg;
  FIMEMORY *memory;
  uint32_t msize;
  char *mbuf, *buf;
  
  FREE_IMAGE_FORMAT format = FreeImage_GetFileType(path, 0);
  if (format == FIF_UNKNOWN) {
    musicd_log(LOG_ERROR, "image", "unrecognized filetype '%s'", path);
    return NULL;
  }
  
  fromimg = FreeImage_Load(format, path, 0);
  if (!fromimg) {
    musicd_log(LOG_ERROR, "image", "can't open image '%s'", path);
    return NULL;
  }
  
  toimg = FreeImage_MakeThumbnail(fromimg, size, 1);
  FreeImage_Unload(fromimg);

  memory = FreeImage_OpenMemory(NULL, 0);
  
  FreeImage_SaveToMemory(FIF_JPEG, toimg, memory, 0);
  
  FreeImage_AcquireMemory(memory, (BYTE **)&mbuf, &msize);
  
  buf = malloc(msize);
  memcpy(buf, mbuf, msize);
  *data_size = msize;
  
  FreeImage_CloseMemory(memory);
  
  return buf;
}

void *image_album_task(void *data)
{
  char *buf, *name, *cache_name, *path;
  int size;
  image_task_t *task = (image_task_t *)data;

  /* Round to closest power of two */
  task->size = pow(2, ceil(log(task->size)/log(2)));
  
  path = library_album_image_path(task->id);
  
  buf = image_create_thumbnail(path, task->size, &size);
  if (!buf) {
    free(task);
    return NULL;
  }
  
  name = stringf("album%ld", task->id);
  cache_name = image_cache_name(name, task->size);
  free(name);
  
  cache_set(cache_name, buf, size);
  
  free(cache_name);
  
  free(task);
  return NULL;
}

