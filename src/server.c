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
#include "server.h"

#include "client.h"
#include "config.h"
#include "log.h"

#include <netinet/in.h>
#include <netdb.h> 
#include <pthread.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>

static int master_sock = -1;
static pthread_t thread;

static struct client_list_t clients;
static struct pollfd *poll_fds = NULL;
static int poll_nfds = 1, nb_clients = 0;


static void build_pollfds()
{
  int i = 0;
  client_t *client;
  
  if (poll_fds) {
    free(poll_fds);
  }
  
  poll_nfds = nb_clients + 1;
  poll_fds = calloc(poll_nfds, sizeof(struct pollfd));
  
  poll_fds[0].fd = master_sock;
  poll_fds[0].events = POLLIN; 
  
  TAILQ_FOREACH(client, &clients, clients) {
    poll_fds[i + 1].fd = client->fd;
    poll_fds[i + 1].events = POLLIN;
    ++i;
  }
  
}

static client_t *find_client(int i) {
  client_t *client = TAILQ_FIRST(&clients);
  for (; i > 0; --i) {
    client = TAILQ_NEXT(client, clients);
  }
  return client;
}


static void *thread_func(void *data)
{
  (void)data;
  
  int n, i;
  client_t *client;
  
  while (1) {
    n = poll(poll_fds, poll_nfds, -1);
    if (n == -1) {
      musicd_perror(LOG_ERROR, "server", "Could not poll socket(s)");
      continue;
    }
    
    if (n == 0) {
      continue;
    }
    
    if (poll_fds[0].revents & POLLIN) {
      if (server_accept()) {
        continue;
      }
      musicd_log(LOG_INFO, "server", "New client connected.");
    }
    
    /* Go through all clients. If...
     * - incoming data: process data from client.
     *   - if client has track open, start polling POLLOUT too
     * - can write to socket: write next packet from stream.
     *   - if no track is open now, stop polling POLLOUT
     */
    /**
     * @todo FIXME Separate logic for handling pushing data to the client.
     */
    for (i = 0; i < nb_clients; ++i) {
      client = find_client(i);
      if (poll_fds[i + 1].revents & POLLIN) {
        if (client_process(client) == 1) {
          server_del_client(client);
          musicd_log(LOG_INFO, "server", "Client connection closed.");
          break; /* Break, because poll_fds has changed. */
        }

        if (client->stream && client->stream->at_end == 0) {
          poll_fds[i + 1].events = POLLIN | POLLOUT;
        }
        
      } else if (poll_fds[i + 1].revents & POLLOUT) {
        if (client->stream && client->stream->at_end == 0) {
          client_next_packet(client);
        }
        if (!client->stream || client->stream->at_end == 1) {
          poll_fds[i + 1].events = POLLIN;
        }
      }
    }
  }
  return NULL;
}

static int server_bind_tcp(const char *address)
{
  struct sockaddr_in sockaddr;
  struct hostent *host;
  int port;
  
  port = config_to_int("port");
  
  master_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (master_sock < 0) {
    musicd_perror(LOG_ERROR, "server", "Could not open socket.");
    return -1;
  }
  
  if (!address) {
    sockaddr.sin_addr.s_addr = INADDR_ANY;
  } else {
    bzero(&sockaddr, sizeof(sockaddr));
    host = gethostbyname(address);
    if (!host) {
      musicd_log(LOG_ERROR, "server", "Could not resolve address %s.",
                 address);
      close(master_sock);
      master_sock = -1;
      return -1;
    }
    bcopy((char*)host->h_addr, (char*)&sockaddr.sin_addr.s_addr,
          host->h_length);
  }
  
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  
  if (bind(master_sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
    musicd_perror(LOG_ERROR, "server", "Could not bind socket");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  musicd_log(LOG_VERBOSE, "server", "Listening on %s:%i", address, port);

  return 0;
}

static int server_bind_unix(const char *path)
{
  struct sockaddr_un sockaddr;
  
  master_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (master_sock < 0) {
    musicd_perror(LOG_ERROR, "server", "Could not open socket.");
    return -1;
  }
  
  strcpy(sockaddr.sun_path, path);
  sockaddr.sun_family = AF_UNIX;
  
  unlink(sockaddr.sun_path);
  
  if (bind(master_sock, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0) {
    musicd_perror(LOG_ERROR, "server", "Could not bind socket");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  musicd_log(LOG_VERBOSE, "server", "Listening on %s", path);

  return 0;
}

static int server_bind()
{
  const char *bind;
  
  bind = config_get("bind");
  if (strlen(bind) == 0) {
    musicd_perror(LOG_ERROR, "server", "Invalid value for 'bind'.");
    return -1;
  }
  
  if (!strcmp(bind, "any")) {
    return server_bind_tcp(NULL);
  }
  
  if (bind[0] >= '0' && bind[0] <= '9') {
    return server_bind_tcp(bind);
  }
  
  /* It is not 'any', and does not begin with number, assume it is a unix
   * socket path. */
  bind = config_to_path("bind");
  return server_bind_unix(bind);
}

int server_start()
{
  int result;
  
  result = server_bind();
  if (result) {
    return result;
  }
  
  TAILQ_INIT(&clients);
  build_pollfds();
  
  listen(master_sock, 5);
  
  if (pthread_create(&thread, NULL, thread_func, NULL)) {
    musicd_perror(LOG_ERROR, "server", "Could not create thread");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  return 0;
}

int server_accept()
{
  int sock;
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(struct sockaddr_in);
  client_t* client;
  sock = accept(master_sock, (struct sockaddr*)&cli_addr, &clilen);
  if (sock < 0) {
    musicd_perror(LOG_ERROR, "server", "Could not accept incoming connection");
    return -1;
  }
  client = client_new(sock);
  server_add_client(client);
  return 0;
}

void server_add_client(client_t *client)
{
  TAILQ_INSERT_TAIL(&clients, client, clients);
  ++nb_clients;
  build_pollfds();
}

void server_del_client(client_t *client)
{ 
  TAILQ_REMOVE(&clients, client, clients);
  client_close(client);
  --nb_clients;
  build_pollfds();
}

