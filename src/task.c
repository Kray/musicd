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

#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

/*
 * thread = any thread running tasks
 * worker = task that is marked as a worker
 *
 * In total MAX_ACTIVE_THREADS can be running simultaneously, but only
 * MAX_ACTIVE_WORKERS of those can be running worker tasks.
 *
 * This means that too many CPU intensive tasks (like image scaling) can't be
 * running simultaneously locking up the system, but tasks that are mostly
 * waiting (like lyrics fetching) can be doing their waiting without disturbing
 * tasks that actually need resources.
 */

#define MAX_ACTIVE_WORKERS 4
#define MAX_ACTIVE_THREADS 32

static pthread_mutex_t task_mutex = PTHREAD_MUTEX_INITIALIZER;
static task_t *next_task = NULL, *last_task = NULL;
static int active_workers = 0, active_threads = 0;

static void *thread_func(void *data)
{
  (void)data;
  task_t *task;

  while (1) {
    pthread_mutex_lock(&task_mutex);

    for (task = next_task; task; task = task->next) {
      if (!task->worker || active_workers < MAX_ACTIVE_WORKERS) {
        /* Task is not worker or worker limit not reached */
        break;
      }
    }

    if (!task) {
      --active_threads;
      musicd_log(LOG_DEBUG, "task",
                 "quitting thread (%d/%d workers of %d/%d remaining)",
                 active_workers, MAX_ACTIVE_WORKERS,
                 active_threads, MAX_ACTIVE_THREADS);
      pthread_mutex_unlock(&task_mutex);
      return NULL;
    }

    if (task->prev) {
      task->prev->next = task->next;
    }
    if (task->next) {
      task->next->prev = task->prev;
    }
    if (task == next_task) {
      next_task = task->next;
    }
    if (task == last_task) {
      last_task = task->prev;
    }

    if (task->worker) {
      ++active_workers;
      musicd_log(LOG_DEBUG, "task", "%p starting worker %d/%d", task,
                 active_workers, MAX_ACTIVE_WORKERS);
    } else {
      musicd_log(LOG_DEBUG, "task", "%p starting", task);
    }

    pthread_mutex_unlock(&task_mutex);
    task->func(task->data);
    pthread_mutex_lock(&task_mutex);

    if (task->worker) {
      --active_workers;
      musicd_log(LOG_DEBUG, "task", "%p finished worker (%d/%d remain)", task,
                 active_workers, MAX_ACTIVE_WORKERS);
    } else {
      musicd_log(LOG_DEBUG, "task", "%p finished", task);
    }

    pthread_mutex_unlock(&task_mutex);

    if (!task->detached) {
      write(task->pipe[1], "\0", 1);
    } else {
      task_free(task);
    }
  }

}


static void start(task_t *task)
{
  pthread_t thread;

  if (!last_task) {
    next_task = last_task = task;
  } else {
    last_task->next = task;
    task->prev = last_task;
    last_task = task;
  }

  if (active_threads < MAX_ACTIVE_THREADS
   && active_workers < MAX_ACTIVE_WORKERS) {
    musicd_log(LOG_DEBUG, "task", "%p spawning thread %d/%d", task,
             active_threads + 1, MAX_ACTIVE_THREADS);

    if (pthread_create(&thread, NULL, thread_func, NULL)) {
      musicd_perror(LOG_FATAL, "task", "pthread_create: ");
      abort();
    }
    pthread_detach(thread);

    ++active_threads;
  } else {
    musicd_log(LOG_DEBUG, "task", "%p queued");
  }
}


task_t *task_new()
{
  task_t *task = malloc(sizeof(task_t));
  memset(task, 0, sizeof(task_t));
  pipe(task->pipe);

  return task;
}

void task_start(task_t *task)
{
  pthread_mutex_lock(&task_mutex);
  start(task);
  pthread_mutex_unlock(&task_mutex);
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

void task_launch(task_t *task)
{
  pthread_mutex_lock(&task_mutex);
  task->detached = 1;
  start(task);
  pthread_mutex_unlock(&task_mutex);
}
