#include "internal.h"

/* "chunks" are the generic concept used to push "bulk" data into the final
squashfs image. All chunks are put into the image in the exact order they
were submitted, but they may be distributed to multiple threads for processing.
*/

static inline void
libsqfs_chunk_list_push_back(libsqfs_chunk_list * list, libsqfs_chunk * chunk)
{
	chunk->prev = list->last;
	chunk->next = 0;
	if (list->last) list->last->next = chunk;
	else list->first = chunk;
	list->last = chunk;
}

static inline void
libsqfs_chunk_list_remove(libsqfs_chunk_list * list, libsqfs_chunk * chunk)
{
	if (chunk->prev) chunk->prev->next = chunk->next;
	else list->first = chunk->next;
	if (chunk->next) chunk->next->prev = chunk->prev;
	else list->last = chunk->prev;
}

static inline libsqfs_chunk *
libsqfs_chunk_list_pop_front(libsqfs_chunk_list * list)
{
	libsqfs_chunk * chunk = list->first;
	if (!chunk) return 0;
	libsqfs_chunk_list_remove(list, chunk);
	return chunk;
}

void
libsqfs_image_submit_chunk(libsqfs_image_t image, libsqfs_chunk * chunk)
{
	pthread_mutex_lock(&image->chunks.lock);
	libsqfs_chunk_list_push_back(&image->chunks.submitted, chunk);
	chunk->image = image;
	chunk->state = libsqfs_chunk_pending;
	image->chunks.nsubmitted ++;
	if (!image->chunks.next_pending) image->chunks.next_pending = chunk;
	pthread_cond_signal(&image->chunks.cond);
	pthread_mutex_unlock(&image->chunks.lock);
}

/* FIXME: more detailed error description would be helpful */
static void
libsqfs_image_chunk_error(libsqfs_image_t image, libsqfs_chunk * chunk)
{
	pthread_mutex_lock(&image->chunks.lock);
	/* "finished" also includes an error state; propagation of
	the "reason" for the error needs to be done differently */
	chunk->state = libsqfs_chunk_finished;
	image->chunks.ncompleted++;
	/* FIXME: return buffer to pool */
	free(chunk->dst.data);
	pthread_cond_broadcast(&image->chunks.cond);
	pthread_mutex_unlock(&image->chunks.lock);
}

static void
libsqfs_image_chunk_compression_done(libsqfs_image_t image, libsqfs_chunk * chunk)
{
	pthread_mutex_lock(&image->chunks.lock);
	chunk->state = libsqfs_chunk_compressed;
	bool something_written = false;
	
	/* try to write out this chunk, but honor submission order; if "later"
	blocks were compressed out-of-order, have to pick them up now and
	write them out as well */
	while(chunk && chunk->state == libsqfs_chunk_compressed) {
		if (chunk->prev && chunk->prev->state != libsqfs_chunk_finished) break;
		
		chunk->dst.offset = libsqfs_image_reserve(image, chunk->dst.size);
		pthread_mutex_unlock(&image->chunks.lock);
		
		ssize_t written = libsqfs_pwrite(image->dst, chunk->dst.data, chunk->dst.size, chunk->dst.offset);
		
		pthread_mutex_lock(&image->chunks.lock);
		/* FIXME: verify number of bytes written */
		(void)written;
		chunk->state = libsqfs_chunk_finished;
		/* FIXME: return buffer to pool */
		free(chunk->dst.data);
		image->chunks.ncompleted++;
		chunk = chunk->next;
		
		something_written = true;
	}
	
	if (something_written) pthread_cond_broadcast(&image->chunks.cond);
	
	pthread_mutex_unlock(&image->chunks.lock);
}

static bool
libsqfs_image_compress_chunk(libsqfs_image_t image, libsqfs_chunk * chunk)
{
	/* FIXME: should not malloc, but retrieve from a pool instead: For
	the size of objects passed around here, malloc is very mmap-happy.
	
	When using a pool there is a potential for deadlock here: Suppose
	chunks A, B and C have been submitted and multiple threads race
	to process them. If buffers are allocated for B and C, there may
	be no buffers available for A, but no buffer will ever be freed
	because write-out is blocked on A.
	
	To alleviate this problem, allocation must be performed when
	acquiring the chunk for compression. (One other option would
	be allowing allocations if the pool is drained, but IMHO this
	makes the pool somewhat pointless and more seriously precludes
	setting an upper hard limit for the memory usage). */
	chunk->dst.data = malloc(chunk->uncompressed_size);
	if (!chunk->dst.data) return false;
	
	ssize_t count = libsqfs_data_pread(chunk->src.data, chunk->dst.data, chunk->uncompressed_size, chunk->src.offset);
	if (count != chunk->uncompressed_size) return false;
	
	/* currently, no compression is done, obviously */
	chunk->dst.size = chunk->uncompressed_size;
	chunk->dst.compressed = false;
	
	return true;
}

static void
libsqfs_image_process_chunks_unlocked(libsqfs_image_t image)
{
	/* FIXME: need a "fast-exit-on-error" */
	for(;;) {
		while (!image->chunks.done && !image->chunks.next_pending)
			pthread_cond_wait(&image->chunks.cond, &image->chunks.lock);
		
		libsqfs_chunk * chunk = image->chunks.next_pending;
		if (!chunk) break;
		image->chunks.next_pending = chunk->next;
		
		pthread_mutex_unlock(&image->chunks.lock);
		
		bool success = libsqfs_image_compress_chunk(image, chunk);
		if (!success) libsqfs_image_chunk_error(image, chunk);
		else libsqfs_image_chunk_compression_done(image, chunk);
		
		pthread_mutex_lock(&image->chunks.lock);
	}
}

void
libsqfs_image_process_chunks(libsqfs_image_t image)
{
	pthread_mutex_lock(&image->chunks.lock);
	libsqfs_image_process_chunks_unlocked(image);
	pthread_mutex_unlock(&image->chunks.lock);
}

void
libsqfs_finish_chunks(libsqfs_image_t image)
{
	pthread_mutex_lock(&image->chunks.lock);
	
	image->chunks.done = true;
	
	libsqfs_image_process_chunks_unlocked(image);
	
	while(image->chunks.nsubmitted != image->chunks.ncompleted)
		pthread_cond_wait(&image->chunks.cond, &image->chunks.lock);
	
	pthread_mutex_unlock(&image->chunks.lock);
}

libsqfs_chunk *
libsqfs_image_submit_chunk_for_data(libsqfs_image_t image, libsqfs_data_t data,
	libsqfs_off_t offset, size_t size)
{
	libsqfs_chunk * chunk = malloc(sizeof(*chunk));
	if (!chunk) return 0;
	
	chunk->uncompressed_size = size;
	chunk->src.data = data;
	chunk->src.offset = 0;
	
	libsqfs_image_submit_chunk(image, chunk);
	
	return chunk;
}
