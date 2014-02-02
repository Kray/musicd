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

#include <arpa/inet.h>
#include <fcntl.h>
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

#define MAX_CLIENTS 1024

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
  
  TAILQ_FOREACH(client, &clients, clients) {
    poll_fds[i].fd = client_poll_fd(client);
    poll_fds[i].events = client_poll_events(client);

    ++i;
  }
  
  poll_fds[i].fd = master_sock;
  poll_fds[i].events = POLLIN;
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
  int n, i;
  client_t *client;
  
  (void)data;
  
  signal(SIGPIPE, SIG_IGN);
  
  while (1) {
    n = poll(poll_fds, poll_nfds, -1);

    if (n == -1) {
      musicd_perror(LOG_ERROR, "server", "can't poll socket(s)");
      continue;
    }
    
    if (n == 0) {
      continue;
    }
    
    if (poll_fds[nb_clients].revents & POLLIN) {
      if ((client = server_accept())) {
        musicd_log(LOG_INFO, "server", "new client from %s", client->address);
      }
      continue; /* poll_fds changed */
    }

    for (i = 0; i < nb_clients; ++i) {
      if (poll_fds[i].revents & POLLIN
       || poll_fds[i].revents & POLLOUT) {
        client = find_client(i);
        if (client_process(client)) {
          musicd_log(LOG_INFO, "server", "client from %s disconnected",
                     client->address);
          server_del_client(client);
          break; /* Break, because poll_fds has changed. */
        }

        poll_fds[i].fd = client_poll_fd(client);
        poll_fds[i].events = client_poll_events(client);
      }
    }
  }
  return NULL;
}

static int server_bind_tcp(const char *address)
{
  struct sockaddr_in sockaddr;
  struct hostent *host;
  int port, value;
  
  port = config_to_int("port");
  
  master_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (master_sock < 0) {
    musicd_perror(LOG_ERROR, "server", "can't open socket");
    return -1;
  }
  
  /* Reuse the address even if it is in TIME_WAIT state */
  value = 1;
  setsockopt(master_sock, SOL_SOCKET, SO_REUSEADDR,
             (void *)&value, sizeof(value));
  
  if (!address) {
    sockaddr.sin_addr.s_addr = INADDR_ANY;
  } else {
    bzero(&sockaddr, sizeof(sockaddr));
    host = gethostbyname(address);
    if (!host) {
      musicd_log(LOG_ERROR, "server", "can't resolve address %s", address);
      close(master_sock);
      master_sock = -1;
      return -1;
    }
    bcopy((char *)host->h_addr_list[0], (char *)&sockaddr.sin_addr.s_addr,
          host->h_length);
  }
  
  sockaddr.sin_family = AF_INET;
  sockaddr.sin_port = htons(port);
  
  if (bind(master_sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    musicd_perror(LOG_ERROR, "server", "can't bind socket");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  musicd_log(LOG_VERBOSE, "server", "listening on %s:%d", address ? address : "", port);

  return 0;
}

static int server_bind_unix(const char *path)
{
  struct sockaddr_un sockaddr;
  
  master_sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (master_sock < 0) {
    musicd_perror(LOG_ERROR, "server", "can't open socket");
    return -1;
  }
  
  strcpy(sockaddr.sun_path, path);
  sockaddr.sun_family = AF_UNIX;
  
  unlink(sockaddr.sun_path);
  
  if (bind(master_sock, (struct sockaddr *)&sockaddr, sizeof(sockaddr)) < 0) {
    musicd_perror(LOG_ERROR, "server", "can't bind socket");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  musicd_log(LOG_VERBOSE, "server", "listening on %s", path);

  return 0;
}

static int server_bind()
{
  const char *bind;
  
  bind = config_get("bind");
  if (strlen(bind) == 0) {
    musicd_perror(LOG_ERROR, "server", "invalid value for 'bind'");
    return -1;
  }
  
  if (!strcmp(bind, "any")) {
    return server_bind_tcp(NULL);
  }
  
  if (bind[0] >= '0' && bind[0] <= '9') {
    return server_bind_tcp(bind);
  }
  
  /* It is not 'any', and does not begin with a number, assume it is a unix
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
  
  if (listen(master_sock, SOMAXCONN)) {
    musicd_perror(LOG_ERROR, "server", "listen: ");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  if (pthread_create(&thread, NULL, thread_func, NULL)) {
    musicd_perror(LOG_ERROR, "server", "can't create thread");
    close(master_sock);
    master_sock = -1;
    return -1;
  }
  
  return 0;
}

client_t *server_accept()
{
  int fd, flags;
  struct sockaddr_in cli_addr;
  socklen_t clilen = sizeof(struct sockaddr_in);
  client_t *client;

  fd = accept(master_sock, (struct sockaddr *)&cli_addr, &clilen);
  if (fd < 0) {
    musicd_perror(LOG_ERROR, "server", "can't accept incoming connection");
    abort();
    return NULL;
  }

  if (nb_clients + 1 > MAX_CLIENTS) {
    musicd_log(LOG_VERBOSE, "server",
               "MAX_CLIENTS reached (%d > %d), terminating new client",
               nb_clients + 1, MAX_CLIENTS);
    close(fd);
    return NULL;
  }

  flags = fcntl(fd, F_GETFL, 0);
  fcntl(fd, F_SETFL, flags | O_NONBLOCK);

  client = client_new(fd);
  client->address = malloc(INET6_ADDRSTRLEN);
  client->address[0] = '\0';
  inet_ntop(cli_addr.sin_family, &(cli_addr.sin_addr), client->address,
            INET6_ADDRSTRLEN);
  server_add_client(client);
  return client;
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

