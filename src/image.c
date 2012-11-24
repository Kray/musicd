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

char *image_cache_name(int64_t image, int size)
{ 
  /* Round to closest power of two */
  size = pow(2, ceil(log(size)/log(2)));
  return stringf("%" PRId64 "_%d.jpg", image, size);
}


char *image_create_thumbnail(const char *path, int size, int *data_size)
{
  FIBITMAP *img1, *img2;
  FIMEMORY *memory;
  double ratio;
  uint32_t msize;
  char *mbuf, *buf;
  
  FREE_IMAGE_FORMAT format = FreeImage_GetFileType(path, 0);
  if (format == FIF_UNKNOWN) {
    musicd_log(LOG_ERROR, "image", "unrecognized filetype '%s'", path);
    return NULL;
  }
  
  img1 = FreeImage_Load(format, path, 0);
  if (!img1) {
    musicd_log(LOG_ERROR, "image", "can't open image '%s'", path);
    return NULL;
  }

  ratio = FreeImage_GetWidth(img1) / (double) FreeImage_GetHeight(img1);
  if (ratio >= 1.75) {
    /* The image has so big width/height ratio that it is most likely a scan
     * of multiple sheets placed horizontally. Crop it so that only the part
     * at the right border is left */
    img2 = FreeImage_Copy(img1,
                          FreeImage_GetWidth(img1) -
                            FreeImage_GetWidth(img1) / round(ratio),
                          0,
                          FreeImage_GetWidth(img1),
                          FreeImage_GetHeight(img1));
    if (!img2) {
      if (!img1) {
        musicd_log(LOG_ERROR, "image", "can't crop image '%s'", path);
        return NULL;
      }
    }
    FreeImage_Unload(img1);
    img1 = img2;
  }

  img2 = FreeImage_MakeThumbnail(img1, size, 1);
  if (!img2) {
    musicd_log(LOG_ERROR, "image", "can't scale image '%s'", path);
    return NULL;
  }

  FreeImage_Unload(img1);
  img1 = img2;

  memory = FreeImage_OpenMemory(NULL, 0);
  
  FreeImage_SaveToMemory(FIF_JPEG, img1, memory, 0);
  
  FreeImage_AcquireMemory(memory, (BYTE **)&mbuf, &msize);
  
  buf = malloc(msize);
  memcpy(buf, mbuf, msize);
  *data_size = msize;
  
  FreeImage_CloseMemory(memory);
  
  return buf;
}


void *image_task(void *data)
{
  char *buf = NULL, *cache_name, *path;
  int size = 0;
  image_task_t *task = (image_task_t *)data;
  
  /* Round to closest power of two */
  task->size = pow(2, ceil(log(task->size)/log(2)));
  
  cache_name = image_cache_name(task->id, task->size);
  
  path = library_image_path(task->id);
  if (path) {
    buf = image_create_thumbnail(path, task->size, &size);
  }

  cache_set(cache_name, buf, size);
  
  free(path);
  free(buf);
  free(cache_name);
  free(task);
  return NULL;
}

