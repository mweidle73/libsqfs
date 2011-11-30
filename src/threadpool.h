/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2010
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Refer to the file "COPYING" for details.
 */

#ifndef LIBSQFS_THREADPOOL_H
#define LIBSQFS_THREADPOOL_H

#include <libsqfs.h>

/* worker thread pool */

typedef struct libsqfs_worker_thread libsqfs_worker_thread;
typedef struct libsqfs_threadpool libsqfs_threadpool;
typedef struct libsqfs_workitem libsqfs_workitem;

struct libsqfs_threadpool {
	struct { libsqfs_worker_thread * first, * last; } threads;
};

void
libsqfs_threadpool_init(libsqfs_threadpool * threadpool);

void
libsqfs_threadpool_fini(libsqfs_threadpool * threadpool);

bool
libsqfs_threadpool_spawn_worker(libsqfs_threadpool * threadpool,
	void * (*function)(void * closure),
	void * closure);

size_t
libsqfs_threadpool_auto_spawn_worker(libsqfs_threadpool * threadpool,
	void * (*function)(void * closure),
	void * closure);

/* wait until all threads exit */
void
libsqfs_threadpool_wait(libsqfs_threadpool * threadpool);

#endif
