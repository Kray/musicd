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
#ifndef MUSICD_URL
#define MUSICD_URL

/* Convenience functions on top of curl. */

/**
 * Fetch @p url blockingly. Return value must be freed later.
 */
char *url_fetch(const char *url);

char *url_escape(const char *string);

char *url_escape_location(const char *server, const char *location);

/**
 * Connects to @p server (e.g. http://example.com/) and fetches ESCAPED
 * page in @p location. Return value must be freed later. */
char *url_fetch_escaped_location(const char *server,  const char *location);

#endif
