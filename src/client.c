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
#include "client.h"
#include "config.h"
#include "image.h"
#include "library.h"
#include "log.h"
#include "lyrics.h"
#include "server.h"
#include "strings.h"
#include "task.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/poll.h>


static int read_data(client_t *client)
{
  char buffer[1025];
  int n;

  n = read(client->fd, buffer, 1024);
  if (n == 0) {
    musicd_log(LOG_INFO, "client", "%s: exiting", client->address);
    return -1;
  }
  if (n < 0) {
    if (errno == EWOULDBLOCK) {
      /* No data available right now, ignore */
      return 0;
    }

    musicd_perror(LOG_INFO, "client", "%s: can't read", client->address);
    return -1;
  }

  string_nappend(client->inbuf, buffer, n);

  return n;
}


static int write_data(client_t *client)
{
  int n;
  
  n = write(client->fd, string_string(client->outbuf), string_size(client->outbuf));
  if (n < 0) {
    if (errno == EWOULDBLOCK) {
      /* It would block right now, ignore */
      return 0;
    }

    musicd_perror(LOG_INFO, "client", "%s: can't write data", client->address);
    return -1;
  }

  string_remove_front(client->outbuf, n);

  return 0;
}


static void find_protocol(client_t *client)
{
  protocol_t **protocol;

  for (protocol = protocols; *protocol != NULL; ++protocol) {
    if ((*protocol)->detect(string_string(client->inbuf),
                            string_size(client->inbuf)) == 1) {
      break;
    }
  }
  client->protocol = *protocol;
}

client_t *client_new(int fd)
{
  client_t *result = malloc(sizeof(client_t));
  memset(result, 0, sizeof(client_t));

  result->fd = fd;
  result->inbuf = string_new();
  result->outbuf = string_new();

  return result;
}

void client_close(client_t *client)
{
  if (client->protocol) {
    client->protocol->close(client->self);
  }
  close(client->fd);
  free(client->address);
  string_free(client->inbuf);
  string_free(client->outbuf);
  free(client);
}

int client_poll_fd(client_t *client)
{
  if (client->state == CLIENT_STATE_WAIT_TASK) {
    return task_pollfd(client->wait_task);
  }
  return client->fd;
}

int client_poll_events(client_t *client)
{
  int events = 0;

  if (client->state == CLIENT_STATE_NORMAL
   || client->state == CLIENT_STATE_FEED
   || client->state == CLIENT_STATE_WAIT_TASK) {
    events |= POLLIN;
  }

  if (string_size(client->outbuf) > 0
   || client->state == CLIENT_STATE_FEED
   || client->state == CLIENT_STATE_DRAIN) {
    events |= POLLOUT;
  }

  return events;
}


bool client_has_data(client_t *client)
{
  if (string_size(client->outbuf) > 0 || client->state == CLIENT_STATE_FEED) {
    return true;
  }
  return false;
}

int client_process(client_t *client)
{
  int result;

  result = read_data(client);
  if (result < 0) {
    return result;
  }

  if (!client->protocol) {
    /* The client has no protocol detected yet */

    find_protocol(client);

    if (!client->protocol) {
      musicd_log(LOG_ERROR, "client", "%s: unknown protocol, terminating",
                 client->address);
      return -1;
    }
    
    musicd_log(LOG_DEBUG, "client", "%s: protocol is '%s'",
                          client->address, client->protocol->name);
    
    /* Actually open the client to be processed with detected protocol */
    client->self = client->protocol->open(client);
  }

  if (client->state == CLIENT_STATE_WAIT_TASK) {
    /* Client was waiting for task to finish and now the task manager signaled
     * through the pipe. */
    client->state = CLIENT_STATE_NORMAL;
    task_free(client->wait_task);
    if (client->wait_callback(client->self, client->wait_data) < 0) {
      return -1;
    }
  }

  /* (Try to) purge the entire outgoing buffer. */

  if (string_size(client->outbuf) > 0) {
    /* There is outgoing data in buffer, try to write */
    result = write_data(client);
    if (result < 0) {
      return result;
    }
  }

  if (client->state == CLIENT_STATE_DRAIN) {
    if (string_size(client->outbuf) == 0) {
      /* Client was draining, and now it is done - terminate */
      return -1;
    }
  }

  /* If there was nothing to write but we have unprocessed data, process it. */

  if (string_size(client->inbuf) > 0) {
    result = client->protocol->process(client->self,
                                      string_string(client->inbuf),
                                      string_size(client->inbuf));
    if (result < 0) {
      return result;
    }

    string_remove_front(client->inbuf, result);
  } else if (client->state == CLIENT_STATE_FEED
          && string_size(client->outbuf) == 0) {

    /* There wasn't anything to process, we can push data to the client and the
     * outgoing buffer is empty. */

    result = client->protocol->feed(client->self);
    if (result < 0) {
      return result;
    }
  }

  return 0;
}

int client_send(client_t *client, const char *format, ...)
{
  int n, size = 128;
  char *buf;
  va_list va_args;

  buf = malloc(size);
  
  while (1) {
    va_start(va_args, format);
    n = vsnprintf(buf, size, format, va_args);
    va_end(va_args);
    
    if (n > -1 && n < size) {
      break;
    }
    
    if (n > -1) {
      size = n + 1;
    } else {
      size *= 2;
    }
    
    buf = realloc(buf, size);
  }
  string_append(client->outbuf, buf);
  free(buf);
  return n;
}

int client_write(client_t *client, const char *data, size_t n)
{
  string_nappend(client->outbuf, data, n);
  return n;
}

void client_start_feed(client_t *client)
{
  client->state = CLIENT_STATE_FEED;
}

void client_stop_feed(client_t *client)
{
  client->state = CLIENT_STATE_NORMAL;
}

void client_wait_task(client_t *client, task_t *task,
                      client_callback_t callback, void *data)
{
  client->wait_task = task;
  client->wait_callback = callback;
  client->wait_data = data;
  client->state = CLIENT_STATE_WAIT_TASK;
}

void client_drain(client_t *client)
{
  client->state = CLIENT_STATE_DRAIN;
}

