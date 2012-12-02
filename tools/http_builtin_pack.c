/*
 * This file is part of musicd.
 * Copyright (C) 2012 Matti Virkkunen <mvirkkunen@gmail.com>
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

#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>

char *join_path(char *a, char *b) {
  char *result = malloc(strlen(a) + strlen(b) + 2);
  result[0] = '\0';
  
  strcat(result, a);
  strcat(result, "/");
  strcat(result, b);
  
  return result;
}

void process_file(char *path, char *url) {
  static char buf[4096];
  
  FILE *file = fopen(path, "rb");
  if (!file) {
    fprintf(stderr, "error processing file '%s'", path);
    perror(NULL);
    exit(1);
  }
  
  fseek(file, 0, SEEK_END);
  int length = ftell(file);
  fseek(file, 0, SEEK_SET);
  
  printf("  { .url = \"%s\", .length = %d, .data = \"", url, length);
  
  int len;
  while ((len = fread(buf, 1, sizeof(buf), file)) > 0) {
    char *p;
    for (p = buf; len; len--, p++)
      printf("\\x%02hhx", *p);
  }
  
  printf("\" },\n");
  
  fclose(file);
}

void process_directory(char *path, char *url) {
  DIR *dir = opendir(path);
  if (!dir) {
    fprintf(stderr, "error processing directory '%s'", path);
    perror(NULL);
    exit(1);
  }
  
  struct dirent *entry;
  while ((entry = readdir(dir))) {
    if (entry->d_name[0] == '.')
      continue;
    
    char *child_path = join_path(path, entry->d_name),
         *child_url = join_path(url, entry->d_name);
    
    if (entry->d_type == DT_REG)
      process_file(child_path, child_url);
    else if (entry->d_type == DT_DIR)
      process_directory(child_path, child_url);
    
    free(child_url);
    free(child_path);
  }
  
  closedir(dir);
}

int main(int argc, char **argv) {
  printf("#include <stdlib.h>\n\
#include <string.h>\
\n\
/* This is a generated file. Do not edit by hand. */\n\
\n\
struct file_entry {\n\
  char *url;\n\
  int length;\n\
  char *data;\n\
};\n\
\n\
static const struct file_entry entries[] = {\n\
");

  process_directory((argc == 1 ? "." : argv[1]), "");
  
  printf("  { .url = NULL }\n\
};\n\
\n\
int http_builtin_file(char *url, char **data, int *length) {\n\
  const struct file_entry *entry;\n\
  \n\
  for (entry = entries; entry->url; entry++) {\n\
    if (!strcmp(entry->url, url)) {\n\
      *data = entry->data;\n\
      *length = entry->length;\n\
      return 1;\n\
    }\n\
  }\n\
  \n\
  return 0;\n\
}\n");
  
  return 0;
}
