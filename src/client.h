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
#include "task.h"
#include "track.h"

#include <stdbool.h>
#include <pthread.h>
#include <sys/queue.h>

/** Client state */
typedef enum client_state {
  /** Default state: if incoming data, call protocol->process */
  CLIENT_STATE_NORMAL = 0,
  /** Feeder state: if can write to socket, call protocol->feed */
  CLIENT_STATE_FEED,
  /** Task state: the client is waiting for a task to complete */
  CLIENT_STATE_WAIT_TASK
} client_state_t;

typedef struct client {
  int fd;

  char *address;

  string_t *inbuf;
  string_t *outbuf;

  protocol_t *protocol;
  void *self;
  
  client_state_t state;

  task_t *wait_task;
  int (*wait_callback)(void *self);

  TAILQ_ENTRY(client) clients;
} client_t;
TAILQ_HEAD(client_list_t, client);

client_t *client_new(int fd);
void client_close(client_t *client);

/**
 * @returns file descriptor for polling
 * @note This is not guaranteed to be the actual socket
 */
int client_poll_fd(client_t *client);
/**
 * @returns event types for polling
 */
int client_poll_events(client_t *client);

bool client_has_data(client_t *client);

int client_process(client_t *client);


/* Internal API for protocols, do not use externally */

int client_send(client_t *client, const char *format, ...);
int client_write(client_t *client, const char *data, size_t n);

void client_start_feed(client_t *client);
void client_stop_feed(client_t *client);

void client_wait_task(client_t *client, task_t *task,
                      int (*callback)(void *self));


#endif
