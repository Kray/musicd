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


#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h> 


static int server_socket;

static int open_socket(const char *host, int port)
{
  int sockfd, portno, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Could not open socket");
    return -1;
  }
  server = gethostbyname(host);
  if (server == NULL) {
      fprintf(stderr,"Could not resolve host %s\n", host);
      return -1;
  }
  bzero((char *) &serv_addr, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  bcopy((char *)server->h_addr, 
        (char *)&serv_addr.sin_addr.s_addr,
        server->h_length);
  serv_addr.sin_port = htons(port);
  if (connect(sockfd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0)  {
    perror("Could not open connection");
    return -1;
  }
  return sockfd;
}

static int verbose_flag;

int main(int argc, char *argv[])
{
  
  int c;
     
       while (1)
         {
           static struct option long_options[] =
             {
               /* These options set a flag. */
               {"verbose", no_argument,       &verbose_flag, 1},
               {"brief",   no_argument,       &verbose_flag, 0},
               /* These options don't set a flag.
                  We distinguish them by their indices. */
               {"add",     no_argument,       0, 'a'},
               {"append",  no_argument,       0, 'b'},
               {"delete",  required_argument, 0, 'd'},
               {"create",  required_argument, 0, 'c'},
               {"file",    required_argument, 0, 'f'},
               {0, 0, 0, 0}
             };
           /* getopt_long stores the option index here. */
           int option_index = 0;
     
           c = getopt_long (argc, argv, "abc:d:f:",
                            long_options, &option_index);
     
           /* Detect the end of the options. */
           if (c == -1)
             break;
     
           switch (c)
             {
             case 0:
               /* If this option set a flag, do nothing else now. */
               if (long_options[option_index].flag != 0)
                 break;
               printf ("option %s", long_options[option_index].name);
               if (optarg)
                 printf (" with arg %s", optarg);
               printf ("\n");
               break;
     
             case 'a':
               puts ("option -a\n");
               break;
     
             case 'b':
               puts ("option -b\n");
               break;
     
             case 'c':
               printf ("option -c with value `%s'\n", optarg);
               break;
     
             case 'd':
               printf ("option -d with value `%s'\n", optarg);
               break;
     
             case 'f':
               printf ("option -f with value `%s'\n", optarg);
               break;
     
             case '?':
               /* getopt_long already printed an error message. */
               break;
     
             default:
               abort ();
             }
         }
     
       /* Instead of reporting ‘--verbose’
          and ‘--brief’ as they are encountered,
          we report the final status resulting from them. */
       if (verbose_flag)
         puts ("verbose flag is set");
     
       /* Print any remaining command line arguments (not options). */
       if (optind < argc)
         {
           printf ("non-option ARGV-elements: ");
           while (optind < argc)
             printf ("%s ", argv[optind++]);
           putchar ('\n');
         }
  
  server_socket = open_socket("127.0.0.1", 4321);
  if (server_socket < 0) {
    return -1;
  }
  
  char buf[1025], buf2[128];
  int n;
  int protocol;
  
  sprintf(buf, "hello 1 musicdc\n");
  if (!write(server_socket, buf, strlen(buf))) {
    perror("write");
    return -1;
  }
  n = read(server_socket, buf, 1024);
  if (n < 1) {
    perror("read");
    return -1;
  }
  buf[n - 1] = '\0';
  if (sscanf(buf, "hello %i", &protocol) < 1) {
    fprintf(stderr, "Error: excepted hello [protocolversion], got '%s'\n", buf);
    return -1;
  }
  if (protocol != 1) {
    fprintf(stderr, "Server requested unsupported protocol ('%i', when only '1' is supported)\n", protocol);
    return -1;
  }
  
  sprintf(buf, "auth user kissa2\n");
  if (!write(server_socket, buf, strlen(buf))) {
    perror("write");
    return -1;
  }
  n = read(server_socket, buf, 1024);
  if (n < 1) {
    perror("read");
    return -1;
  }
  buf[n - 1] = '\0';
  if (sscanf(buf, "auth %127s", buf2) < 1) {
    fprintf(stderr, "Error: excepted auth [privileges], got '%s'\n", buf);
    return -1;
  }
  //fprintf(stderr, "Privileges: %s\n", buf2);
  
  sprintf(buf, "list album,track\n");
  if (!write(server_socket, buf, strlen(buf))) {
    perror("write");
    return -1;
  }
  while (1) {
    n = read(server_socket, buf, 1024);
    if (n < 1) {
      perror("read");
      break;
    }
    buf[n - 1] = '\0';
    fprintf(stderr, "%s\n", buf);
  }
  
  close(server_socket);
  return 0;
}