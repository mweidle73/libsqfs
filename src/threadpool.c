#include "internal.h"

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
	
	int error = pthread_create(&thread->handle, 0, function, closure);
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
