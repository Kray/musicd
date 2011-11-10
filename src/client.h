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
#include "track.h"

#include <pthread.h>
#include <sys/queue.h>

typedef struct client {
  int fd;
  
  char *buf;
  int buf_size;
  
  char *user;
  
  track_stream_t *track_stream;
  
  TAILQ_ENTRY(client) clients;
} client_t;

TAILQ_HEAD(client_list_t, client);

client_t* client_new(int fd);

void client_close(client_t *client);

/**
 * Reads and handles an RPC call.
 */
int client_process(client_t *client);

int client_send(client_t *client, const char *msg);

int client_next_packet(client_t *client);

#endif
