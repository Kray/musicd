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
#ifndef MUSICD_TASK_H
#define MUSICD_TASK_H

#include <pthread.h>

typedef struct task {
  void *(*func)(void *);
  void *data;

  int pipe[2];
} task_t;

task_t *task_start(void *(*func)(void *), void *data);
/**
 * @returns file descriptor which will trigger POLLIN event once the task is
 * finished
 */
int task_pollfd(task_t *task);
/**
 * Frees resources allocated by task_start
 * @warning If called before the task is actually finished bad things will
 * happen
 */
void task_free(task_t *task);

void task_launch(void *(*func)(void *), void *data);


#endif
