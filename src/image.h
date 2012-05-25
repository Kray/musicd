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
#ifndef MUSICD_IMAGE_H
#define MUSICD_IMAGE_H

#include <stdint.h>

/**
 * @Returns cache name for image of id @p image of @p size size.
 */
char *image_cache_name(int64_t image, int size);

/**
 * @Returns data of thumbnail of image in @p path which fits in square of size
 * @p size, keeping aspect ratio.
 */
char *image_create_thumbnail(const char *path, int size, int *data_size);

typedef struct image_task {
  int64_t id;
  int size;
} image_task_t;

void *image_album_task(void *data);

#endif
