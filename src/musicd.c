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
#include "musicd.h"

#include "config.h"
#include "libav.h"
#include "library.h"
#include "log.h"
#include "server.h"
#include "track.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


void print_usage(char *arg0)
{
  printf("Usage:\n");
  printf("  %s [CONFIG...] [OPTION]\n\n", arg0);
  printf("musicd, a daemon for indexing and streaming music.\n\n");
  printf("Configuration:\n");
  printf("  --config <PATH>\tConfiguration file path. Default is "
         "~/.musicd/musicd.conf\n\n");
  printf("  --no-config <BOOL>\tIf set to true, no config file is attempted "
         "to read.\n\n");
  printf("  Any configuration option can be passed in format --key value.\n");
  printf("  Refer to doc/musicd.conf on configuration options.\n\n");
  printf("Trailing option:\n");
  printf("  --help\tShow this help and exit.\n");
  printf("  --version\tPrint version.\n");
}

void print_version()
{
  printf("musicd (Music Daemon) 0.0.1\n\n");
  printf("Copyright (C) 2011 Konsta Kokkinen <kray@tsundere.fi>\n\n");
 
  printf("libavformat version: %s\n", LIBAVFORMAT_IDENT);
  printf("libavcodec version: %s\n", LIBAVCODEC_IDENT);
  printf("libavutil version: %s\n", LIBAVUTIL_IDENT);
  
  /** @todo TODO libav supported formats and codecs. */
}

/**
 * Create ~/.musicd/ and ~/.musicd/musicd.conf
 */
static void create_default_paths()
{
  char *home, *path;
  struct stat status;
  FILE *file;
  
  home = getenv("HOME");
  if (!home) {
    musicd_log(LOG_ERROR, "main", "$HOME is not set.");
    return;
  }
  
  path = malloc(strlen(home) + 20 + 1);
  
  sprintf(path, "%s/.musicd/", home);
  if (stat(path, &status)) {
    if (mkdir(path, 0777)) {
      musicd_perror(LOG_ERROR, "main", "Could not create directory %s", path);
      goto exit;
    }
  }
  
  sprintf(path, "%s/.musicd/musicd.conf", home);
  if (stat(path, &status)) {
    file = fopen(path, "w");
    if (!file) {
      musicd_perror(LOG_ERROR, "main", "Could not create file %s", path);
      goto exit;
    }
    fprintf(file,"# See man musicd or example musicd.conf distributed with "
                 "musicd.\n");
    fclose(file);
  }
  
exit:
  free(path);
}

int main(int argc, char* argv[])
{ 
  (void)argc; (void)argv;
  
  config_init();
  /*config_set("log-level", "debug");*/
  config_set_hook("log-level", log_level_changed);
  config_set("config", "~/.musicd/musicd.conf");
  config_set("db-file", "~/.musicd/musicd.db");
  config_set("bind", "any");
  config_set("port", "6800");
  
  if (config_load_args(argc, argv)) {
    musicd_log(LOG_FATAL, "main", "Invalid command line arguments.");
    print_usage(argv[0]);
    return -1;
  }
  
  if (config_get_value("help")) {
    print_usage(argv[0]);
    return 0;
  }
  if (config_get_value("version")) {
    print_version();
    return 0;
  }
  
  create_default_paths();
  
  if (!config_to_bool("no-config")
   && config_load_file(config_to_path("config"))) {
    musicd_log(LOG_FATAL, "main", "Could not read config file.");
    return -1;
  }
  
  /* Reload command line arguments - this is because config file might have
   * overwritten them, and command line has the highest priority. */
  config_load_args(argc, argv);
  
  musicd_log(LOG_INFO, "main", "musicd version 0.0.1");
  
  av_register_all();
  avcodec_register_all();
  
  av_log_set_level(AV_LOG_QUIET);
  
  if (library_open()) {
    musicd_log(LOG_FATAL, "main", "Could not open library.");
    return -1;
  }
  
  if (server_start()) {
    musicd_log(LOG_FATAL, "main", "Could not start server.");
    return -1;
  }
  
  if (!config_get_value("music-directory")) {
    musicd_log(LOG_WARNING, "main", "music-directory not set, scanning will "
                                    "be disabled.");
  } else {
    library_scan(config_to_path("music-directory"));
  }
  
  while (1) {
    sleep(1);
  }
  
  return 0;
}