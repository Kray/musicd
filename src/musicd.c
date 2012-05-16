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

#include "cache.h"
#include "config.h"
#include "libav.h"
#include "library.h"
#include "log.h"
#include "scan.h"
#include "server.h"
#include "strings.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>


void print_usage(char *arg0)
{
  printf("Usage:\n");
  printf("  %s [CONFIG...] [OPTION]\n\n", arg0);
  printf("musicd, music collection indexing and streaming daemon\n\n");
  printf("Configuration:\n");
  printf("  --config <PATH>\tConfiguration file path. Default is "
         "~/.musicd/musicd.conf\n\n");
  printf("  --no-config <BOOL>\tIf set to true, no config file is tried to "
         "read\n\n");
  printf("  Any configuration option can be passed in format --key value.\n");
  printf("  Refer to man page or doc/musicd.conf on configuration options."
         "\n\n");
  printf("Trailing option:\n");
  printf("  --help\tShow this help and exit.\n");
  printf("  --version\tPrint version.\n");
}

void print_version()
{
  printf("musicd (music daemon) %s\n\n", MUSICD_VERSION_STRING);
  printf("Copyright (C) 2011 Konsta Kokkinen <kray@tsundere.fi>\n\n");
 
  printf("libavformat version: %s\n", LIBAVFORMAT_IDENT);
  printf("libavcodec version: %s\n", LIBAVCODEC_IDENT);
  printf("libavutil version: %s\n", LIBAVUTIL_IDENT);
  
  /** @todo TODO libav supported formats and codecs. */
}

/**
 * Hook on "directory" config change. Sets db-file and cache to be inside the
 * directory.
 */
static void directory_changed(char *directory)
{
  char *path;
  directory = config_to_path("directory");
  path = stringf("%s/musicd.db", directory);
  config_set("db-file", path);
  free(path);
  path = stringf("%s/cache", directory);
  config_set("cache-dir", path);
  free(path);
}

/**
 * Check if "db-file" or "cache" begin with "directory". If this is the case
 * ensure "directory" exists.
 */
static void confirm_directory()
{
  const char *directory = config_to_path("directory");
  size_t dir_len = strlen(directory);
  const char *db_file = config_to_path("db-file");
  const char *cache_dir = config_to_path("cache-dir");
  struct stat status;
  
  if ((strlen(db_file) < dir_len || !strncmp(directory, db_file, dir_len))
   || (strlen(cache_dir) < dir_len || !strncmp(directory, cache_dir, dir_len))) {
    if (stat(directory, &status)) {
      if (mkdir(directory, 0777)) {
        musicd_perror(LOG_ERROR, "main", "could not create directory %s", 
                      directory);
      }
    }
  }
}

int main(int argc, char* argv[])
{ 
  config_init();

  config_set_hook("log-level", log_level_changed);
  config_set_hook("log-time-format", log_time_format_changed);
  /*config_set("log-level", "debug");*/

  config_set_hook("directory", directory_changed);
  
  config_set("config", "~/.musicd.conf");
  config_set("directory", "~/.musicd");
  config_set("bind", "any");
  config_set("port", "6800");
  
  if (config_load_args(argc, argv)) {
    musicd_log(LOG_FATAL, "main", "invalid command line arguments");
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
  
  if (!config_to_bool("no-config")
   && config_load_file(config_to_path("config"))) {
    musicd_log(LOG_FATAL, "main", "could not read config file");
    return -1;
  }
  
  /* Reload command line arguments - this is because the config file might have
   * overwritten them, and the command line has the highest priority. */
  config_load_args(argc, argv);
  
  confirm_directory();
  
  musicd_log(LOG_INFO, "main", "musicd version %s", MUSICD_VERSION_STRING);
  
  av_register_all();
  avcodec_register_all();
  
  av_log_set_level(AV_LOG_QUIET);
  
  if (library_open()) {
    musicd_log(LOG_FATAL, "main", "could not open library");
    return -1;
  }
  
  if (cache_open()) {
    musicd_log(LOG_FATAL, "main", "could not open cache");
    return -1;
  }
  
  if (server_start()) {
    musicd_log(LOG_FATAL, "main", "could not start server");
    return -1;
  }
  
  if (!config_get_value("music-directory")) {
    musicd_log(LOG_WARNING, "main", "music-directory not set, no scanning");
  } else {
    scan_start();
  }
  
  while (1) {
    sleep(1);
  }
  
  return 0;
}
