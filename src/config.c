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
#include "config.h"

#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

static char *read_line(FILE *file)
{
  int pos = 0, c, buf_len;
  char *result;
  
  buf_len = 81; /* No one writes lines longer than 80 chars, right? ;) */
  result = calloc(buf_len, sizeof(char));
  
  do {
    if (buf_len <= pos + 1) {
      buf_len *= 2;
      result = realloc(result, buf_len);
    }
    c = fgetc(file);    
    result[pos++] = c;
  } while (c != '\n' && c != EOF);
  
  result[--pos] = '\0';
  
  if (c == EOF) {
    free(result);
    return NULL;
  }
  
  return result;
}

/**
 * @returns Pointer to @p line where first occurence of non-whitespace character
 * (meaning not ' ' or '\t') is found.
 */
static const char *skip_whitespace(const char *line)
{
  for (; (*line == ' ' || *line == '\t') && *line != '\0'; ++line) { }
  return line;
}

static char *read_key(const char *line)
{
  int pos = 0;
  char *result;
  
  line = skip_whitespace(line);
  
  for (; line[pos] != ' ' && line[pos] != '\0' && line[pos] != '\t'; ++pos) { }
  
  result = calloc(pos + 1, sizeof(char));
  memcpy(result, line, pos * sizeof(char));
  
  result[pos] = '\0';
  
  return result;
}

static char *read_value(const char *line)
{
  int pos = 0;
  char *result;
  
  line = skip_whitespace(line);
  
  for (; line[pos] != '\0'; ++pos) { }

  result = calloc(pos + 1, sizeof(char));
  memcpy(result, line, pos * sizeof(char));
  
  result[pos] = '\0';
  
  return result;
}

typedef struct setting {
  char *key;
  char *value;
  
  /* Stores value returned by config_to_path */
  char *path_value;
  
  void (*hook)(char *value);
  
  TAILQ_ENTRY(setting) settings;
} setting_t;

TAILQ_HEAD(setting_list, setting);

static struct setting_list settings;


static setting_t *setting_by_key(const char *key)
{
  setting_t *setting;
  TAILQ_FOREACH(setting, &settings, settings) {
    if (!strcmp(setting->key, key)) {
      return setting;
    }
  }
  return NULL;
}

void config_init()
{
  TAILQ_INIT(&settings);
}

void config_set_hook(const char *key, void (*hook)(char *value))
{
  setting_t *setting = setting_by_key(key);
  if (!setting) {
    config_set(key, "");
    setting = setting_by_key(key);
  }
  setting->hook = hook;
}


int config_load_file(const char *path)
{
  FILE *file;
  char *line, *key, *value;
  int i;
  
  file = fopen(path, "r");
  if (!file) {
    musicd_perror(LOG_ERROR, "config", "can't open config file '%s'", path);
    return -1;
  }
  
  while (1) {
    line = read_line(file);
    if (!line) {
      break;
    }
    
    for (i = 0; ; ++i) {
      if (line[i] == '#' || line[i] == '\0') {
        goto next;
      }
      if (line[i] != ' ' && line[i] != '\t') {
        break;
      }
    }
    
    key = read_key(line);
    if (!key) {
      goto next;
    }
    
    value = read_value(line + strlen(key));
    
    config_set(key, value);
    
    free(key);
    free(value);
    
  next:
    free(line);
  }
  
  return 0;

}


int config_load_args(int argc, char **argv)
{
  int i;
  const char *key, *value;
  
  for (i = 1; i < argc; ++i) {
    key = argv[i];
    if (key[0] != '-' || key[1] != '-') {
      musicd_log(LOG_ERROR, "config", "invalid cmdline flag '%s'", key);
      return -1;
    }
    if (argc > ++i) {
      value = argv[i];
      if (value[0] == '-') {
        /* Next argument seems to be new key, just mark this value to be set */
        --i;
        value = "true";
      }
    } else {
      value = "true";
    }
    config_set(key + 2, value);
  }
  return 0;
}


const char *config_get(const char *key)
{
  const char *result = config_get_value(key);
  return result ? result : "";
}


char *config_get_value(const char *key)
{
  setting_t *setting = setting_by_key(key);
  if (!setting) {
    return NULL;
  }
  return setting->value;
}

char *config_to_path(const char *key)
{
  char *home, *value;
  int str_len;
  setting_t *setting;
  
  setting = setting_by_key(key);
  if (!setting) {
    return NULL;
  }
  
  if (setting->value[0] != '~') {
    return setting->value;
  }
  
  value = setting->value + 1;
  
  home = getenv("HOME");
  if (!home) {
    musicd_log(LOG_ERROR, "config", "$HOME not set");
    return NULL;
  }
  
  /* If trailing /, remove it. */
  if (home[strlen(home) - 1] == '/') {
    home[strlen(home) - 1] = '\0';
  }
  
  /* If / right after tilde, handle it too. */
  if (value[0] == '/') {
    ++value;
  }
  
  if (setting->path_value) {
    free(setting->path_value);
  }
  
  str_len = strlen(home) + strlen(value) + 2;
  
  setting->path_value = calloc(str_len, sizeof(char));
  snprintf(setting->path_value, str_len, "%s/%s", home, value);
  
  return setting->path_value;
}

int config_to_int(const char *key)
{
  int result = 0;
  setting_t *setting = setting_by_key(key);
  if (!setting) {
    return 0;
  }
  sscanf(setting->value, "%d", &result);
  return result;
}

int config_to_bool(const char *key)
{
  setting_t *setting = setting_by_key(key);
  if (!setting) {
    return 0;
  }
  if (!strcmp("false", setting->value)) {
    return 0;
  }
  return 1;
}


void config_set(const char *key, const char *value)
{
  setting_t *setting = setting_by_key(key);
  if (!setting) {
    musicd_log(LOG_DEBUG, "config", "new setting: %s %s", key, value);
    setting = malloc(sizeof(setting_t));
    memset(setting, 0, sizeof(setting_t));
    setting->key = strdup(key);
    TAILQ_INSERT_TAIL(&settings, setting, settings);
  } else {
    musicd_log(LOG_DEBUG, "config", "set setting: %s %s", key, value);
    free(setting->value);
  }
  
  setting->value = strdup(value);
  
  if (setting->hook) {
    setting->hook(setting->value);
  }
}


