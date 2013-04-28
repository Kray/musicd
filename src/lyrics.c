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
#include "lyrics.h"

#include "library.h"
#include "log.h"
#include "strings.h"
#include "url.h"

#include <string.h>

lyrics_t *lyrics_new()
{
  lyrics_t *lyrics = malloc(sizeof(lyrics_t));
  memset(lyrics, 0, sizeof(lyrics_t));
  return lyrics;
}

void lyrics_free(lyrics_t *lyrics)
{
  if (!lyrics) {
    return;
  }

  free(lyrics->lyrics);
  free(lyrics->provider);
  free(lyrics->source);
  free(lyrics);
}

/** LyricsWiki parsing strategy:
 * - start from first hit of "<div class='lyricbox'>"
 * - search until first html entity
 * - start converting entities to utf-32 and <br />s to newlines
 * - if over 48 characters without html entity (allows up to 8 <br />s, for
 *   instance), stop
 */
static char *parse_lyrics_page(char *page)
{
  char *p;
  int gap = 0;
  uint32_t chr;
  char *tmp;
  string_t *string = string_new(), *result;
  
  p = strstr(page, "<div class='lyricbox'>");
  
  if (!p) {
    return NULL;
  }
  
  for (; *p != '\0'; ++p) {
    if (string_size(string) > 0 && gap > 48) {
      break;
    }
    
    ++gap;
    
    if (!strncmp(p, "&#", 2)) {
      if (sscanf(p + 2, "%d;", &chr) < 1) {
        continue;
      }
      
      gap = 0;
      
      tmp = (char *)&chr;
      string_push_back(string, tmp[0]);
      string_push_back(string, tmp[1]);
      string_push_back(string, tmp[2]);
      string_push_back(string, tmp[3]);
    } else if (!strncmp(p, "<br />", 6)) {
      string_push_back(string, '\n');
      string_push_back(string, '\0');
      string_push_back(string, '\0');
      string_push_back(string, '\0');
    }
  }
  
  result = string_iconv(string, "UTF-8", "UTF-32");
  string_free(string);
  return string_release(result);
}

static lyrics_t *handle_lyrics_page(const char *page_name)
{
  char *url, *page, *lyrics;
  lyrics_t *result;

  url = url_escape_location("http://lyrics.wikia.com", page_name);
  page = url_fetch(url);
  if (!page) {
    musicd_log(LOG_ERROR, "lyrics", "can't fetch lyrics page");
    free(url);
    return NULL;
  }
  
  lyrics = parse_lyrics_page(page);
  free(page);

  if (!lyrics) {
    free(url);
    return NULL;
  }

  result = lyrics_new();
  result->lyrics = lyrics;
  result->provider = strdup("LyricWiki");
  result->source = url;
  return result;
}

static char *find_lyrics_page_name(const char *page, const char *title)
{
  const char *p, *end;
  char *result;
  
  /* Exact match */
  p = strstr(page, title);
  if (!p) {
    /* Case-insensitive match */
    p = strcasestr(page, title);
    if (!p) {
      return NULL;
    }
  }
  
  for (; p != page && *p != '\n'; --p) { }
  ++p;
  end = strchr(p, '\n');
  if (!end) {
    end = p + strlen(p);
  }
  
  result = malloc(end - p + 1);
  memcpy(result, p, end - p);
  result[end - p] = '\0';

  return result;
}


/**
 * LyricsWiki fetching
 */
lyrics_t *lyrics_fetch(const track_t *track)
{
  char *url, *page, *page_name;
  char *p;
  lyrics_t *lyrics;

  /* Try the exact page */

  page_name = stringf("%s:%s", track->artist, track->title);
  lyrics = handle_lyrics_page(page_name);
  free(page_name);
  if (lyrics) {
    return lyrics;
  }


  /* Try finding the exact song from API search and try that exact page */
  
  url = stringf(
    "http://lyrics.wikia.com/api.php?func=getArtist&artist=%s&fmt=text",
    track->artist);
  page = url_fetch(url);
  free(url);
  if (!page) {
    musicd_log(LOG_ERROR, "lyrics", "can't fetch artist search");
    return NULL;
  }
  
  page_name = find_lyrics_page_name(page, track->title);
  if (page_name) {
    lyrics = handle_lyrics_page(page_name);
    free(page_name);
    if (lyrics) {
      return lyrics;
    }
  }


  /* Try finding the exact artist name and use that like in the first step */

  p = strchr(page, ':');
  if (p) {
    *p = '\0';
    page_name = stringf("%s:%s", page, track->title);
    lyrics = handle_lyrics_page(page_name);
    free(page_name);
    if (lyrics) {
      return lyrics;
    }
  }
  
  return NULL;
}


static void *task_func(void *args)
{
  int64_t id = *((int64_t *)args);
  track_t *track = library_track_by_id(id);
  lyrics_t *lyrics;

  if (!track) {
    free(args);
    return NULL;
  }

  lyrics = lyrics_fetch(track);
  library_lyrics_set(id, lyrics);

  lyrics_free(lyrics);
  free(args);
  return NULL;
}

task_t *lyrics_task(int64_t track)
{
  int64_t *args = malloc(sizeof(int64_t));
  *args = track;

  return task_new(task_func, args);
}
