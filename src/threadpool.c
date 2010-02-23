#include "internal.h"

static void *
thread_function(void * arg)
{
	libsqfs_image_t image = arg;
	
	libsqfs_image_process_chunks(image);
	
	return 0;
}

ssize_t
libsqfs_image_spawn_threads(libsqfs_image_t image, size_t count)
{
	size_t spawned = 0;
	while(spawned < count) {
		libsqfs_worker_thread * thread = malloc(sizeof(*thread));
		if (!thread) break;
		
		if (pthread_create(&thread->handle, 0, thread_function, image)) {
			free(thread);
			break;
		}
		
		thread->next = image->thread_pool;
		image->thread_pool = thread;
		spawned ++;
	}
	
	return spawned;
}

void
libsqfs_image_waitfor_threads(libsqfs_image_t image)
{
	pthread_mutex_lock(&image->chunks.lock);
	image->chunks.done = true;
	pthread_cond_broadcast(&image->chunks.cond);
	pthread_mutex_unlock(&image->chunks.lock);
	while(image->thread_pool) {
		libsqfs_worker_thread * thread = image->thread_pool;
		image->thread_pool = thread->next;
		
		pthread_join(thread->handle, 0);
		free(thread);
	}
}

