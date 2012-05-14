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


/** LyricsWiki parsing strategy:
 * - start from first hit of "<div class='lyricbox'>"
 * - search until first html entity
 * - start converting entities to utf-32 and <br />s to newlines
 * - if over 24 characters without html entity (allows up to 4 <br />s, for
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
    if (string_size(string) > 0 && gap > 24) {
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

static char *handle_lyrics_page(const char *page_name)
{
  char *page;
  char *lyrics;
  page = url_fetch_escaped_location("http://lyrics.wikia.com", page_name);
  if (!page) {
    musicd_log(LOG_ERROR, "lyrics", "can't fetch lyrics page");
    return NULL;
  }
  
  lyrics = parse_lyrics_page(page);
  free(page);
  return lyrics;
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
char *lyrics_fetch(const track_t *track)
{
  char *url, *page, *page_name;
  char *p;
  char *lyrics;


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

void* lyrics_task(void *data)
{
  track_t *track;
  char *lyrics;
  int64_t id = (int64_t)data;
  
  track = library_track_by_id(id);
  if (!track) {
    return NULL;
  }

  lyrics = lyrics_fetch(track);
  library_lyrics_set(id, lyrics);
  
  return NULL;
}


