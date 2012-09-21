/*
 * This file is part of musicd.
 * Copyright (C) 2011-2012 Konsta Kokkinen <kray@tsundere.fi>
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
#ifndef MUSICD_PROTOCOL_H
#define MUSICD_PROTOCOL_H

#include "libav.h"
#include "stream.h"
#include "track.h"

#include <pthread.h>
#include <sys/queue.h>

struct client;

typedef struct protocol {
  const char *name;
  
  /**
   * @returns -1 if not this protocol, 0 if more data needed, 1 if detected
   */
  int (*detect)(const char *buf, size_t buf_size);
  
  /**
   * @returns protocol's new internal structure for @p client
   */
  void *(*open)(struct client *client);
  
  /**
   * @returns amount of bytes consumed from input buffer or < 0 on failure
   */
  int (*process)(void *self, const char *buf, size_t buf_size);
  
  /**
   * Closes and frees all resources associated with this client.
   */
  void (*close)(void *self);
  
  /**
   * Will be called whenever the client can read data if client->feed is true,
   */
  void (*feed)(void *self);

} protocol_t;

/**
 * Null-terminated list of protocols.
 */
extern protocol_t *protocols[];

#endif
