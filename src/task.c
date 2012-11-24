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
#include "task.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void *start(void *data)
{
  task_t *task = (task_t *)data;
  task->func(task->data);

  /* Write to trigger task_pollfd() */
  write(task->pipe[1], "\0", 1);
  return NULL;
}

task_t *task_start(void *(*func)(void *data), void* data)
{
  task_t *task;
  pthread_t thread;

  task = malloc(sizeof(task_t));
  memset(task, 0, sizeof(task_t));
  task->func = func;
  task->data = data;

  pipe(task->pipe);

  pthread_create(&thread, NULL, start, task);
  pthread_detach(thread);

  return task;
}

int task_pollfd(task_t *task)
{
  return task->pipe[0];
}

void task_free(task_t* task)
{
  close(task->pipe[0]);
  close(task->pipe[1]);
  free(task);
}

void task_launch(void *(*func)(void *data), void* data)
{
  pthread_t thread;
  pthread_create(&thread, NULL, func, data);
  pthread_detach(thread);
}
