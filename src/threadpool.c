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

#include "internal.h"
#include <unistd.h>
#include <signal.h>

struct _libsqfs_worker_thread {
	pthread_t handle;
	libsqfs_worker_thread * prev, * next;
	
	libsqfs_threadpool * threadpool;
};

void
libsqfs_threadpool_init(libsqfs_threadpool * threadpool)
{
	threadpool->threads.first = threadpool->threads.last = 0;
}

void
libsqfs_threadpool_fini(libsqfs_threadpool * threadpool)
{
	libsqfs_threadpool_wait(threadpool);
	
	libsqfs_worker_thread * thread = threadpool->threads.first;
	while(thread) {
		libsqfs_worker_thread * next = thread->next;
		free(thread);
		thread = next;
	}
}

bool
libsqfs_threadpool_spawn_worker(libsqfs_threadpool * threadpool,
	void * (*function)(void * closure),
	void * closure)
{
	libsqfs_worker_thread * thread = malloc(sizeof(*thread));
	if (!thread) return false;
	
	sigset_t fullset, oldset;
	sigfillset(&fullset);
	pthread_sigmask(SIG_SETMASK, &fullset, &oldset);
	int error = pthread_create(&thread->handle, 0, function, closure);
	pthread_sigmask(SIG_SETMASK, &oldset, 0);
	if (error) {
		free(thread);
		return false;
	}
	thread->threadpool = threadpool;
	
	thread->prev = threadpool->threads.last;
	thread->next = 0;
	
	if (threadpool->threads.last) threadpool->threads.last->next = thread;
	else threadpool->threads.first = thread;
	threadpool->threads.last = thread;
	
	return true;
}

size_t
libsqfs_threadpool_auto_spawn_worker(libsqfs_threadpool * threadpool,
	void * (*function)(void * closure),
	void * closure)
{
	unsigned long nprocs = 1;
#ifdef _SC_NPROCESSORS_ONLN
	nprocs = sysconf(_SC_NPROCESSORS_ONLN);
#endif
	size_t spawned = 0;
	while(spawned < nprocs * 2) {
		if (!libsqfs_threadpool_spawn_worker(threadpool, function, closure))
			break;
		spawned ++;
	}
	return spawned;
}

void
libsqfs_threadpool_wait(libsqfs_threadpool * threadpool)
{
	while(threadpool->threads.first) {
		libsqfs_worker_thread * thread = threadpool->threads.first;
		threadpool->threads.first = thread->next;
		
		pthread_join(thread->handle, 0);
		free(thread);
	}
}
