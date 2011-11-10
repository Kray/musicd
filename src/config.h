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
#ifndef MUSICD_CONFIG_H
#define MUSICD_CONFIG_H

void config_init();

/**
 * @p hook will be called when setting @p key changes.
 */
void config_set_hook(const char *key, void (*hook)(char *value));

int config_load_file(const char *path);
int config_load_args(int argc, char **argv);

/**
 * @returns Config value for @p key, or "" if not found.
 */
const char *config_get(const char *key);

/**
 * @returns Config value for @p key, or NULL if not found.
 */
char *config_get_value(const char *key);

/**
 * Resolves possible '~' in beginning of value to $HOME and returns pointer
 * to internal buffer. If no setting @p key is found, returns NULL.
 */
char *config_to_path(const char *key);

/**
 * Converts setting @p key to integer using sscanf and format %d. 0 is returned
 * if there is no such setting or if there is no integer in the beginning of
 * the value.
 */
int config_to_int(const char *key);

int config_to_bool(const char *key);

void config_set(const char *key, const char *value);

#endif
