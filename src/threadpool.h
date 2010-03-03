#ifndef LIBSQFS_THREADPOOL_H
#define LIBSQFS_THREADPOOL_H

#include <libsqfs.h>

/* worker thread pool */

typedef struct _libsqfs_worker_thread libsqfs_worker_thread;
typedef struct _libsqfs_threadpool libsqfs_threadpool;
typedef struct _libsqfs_workitem libsqfs_workitem;

struct _libsqfs_threadpool {
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
