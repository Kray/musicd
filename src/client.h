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
#ifndef MUSICD_CLIENT_H
#define MUSICD_CLIENT_H

#include "libav.h"
#include "protocol.h"
#include "stream.h"
#include "strings.h"
#include "track.h"

#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>

typedef struct client {
  int fd;

  char *address;

  string_t *inbuf;
  string_t *outbuf;

  protocol_t *protocol;
  void *self;
  
  /**
   * If true, protocol->feed will be called whenever the client can read data.
   */
  bool feed;

  TAILQ_ENTRY(client) clients;
} client_t;
TAILQ_HEAD(client_list_t, client);

client_t *client_new(int fd);
void client_close(client_t *client);

int client_process(client_t *client);

int client_send(client_t *client, const char *format, ...);
int client_write(client_t *client, const char *data, size_t n);

bool client_has_data(client_t *client);


#endif
