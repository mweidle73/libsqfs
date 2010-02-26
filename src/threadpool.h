#ifndef LIBSQFS_THREADPOOL_H
#define LIBSQFS_THREADPOOL_H

#include <libsqfs.h>

/* worker thread pool */

typedef struct _libsqfs_worker_thread libsqfs_worker_thread;

struct _libsqfs_worker_thread {
	pthread_t handle;
	libsqfs_worker_thread * next;
};

void
libsqfs_image_waitfor_threads(libsqfs_image_t image);

#endif
