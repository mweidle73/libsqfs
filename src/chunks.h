#ifndef LIBSQFS_CHUNKS_H
#define LIBSQFS_CHUNKS_H

#include <libsqfs.h>

/* chunks, representing bulk data (either as complete blocks or fragments)
to be written to the image */

typedef enum {
	libsqfs_chunk_pending,
	libsqfs_chunk_compressed,
	libsqfs_chunk_finished
} libsqfs_chunk_state_t;

typedef struct _libsqfs_chunk libsqfs_chunk;
struct _libsqfs_chunk {
	libsqfs_chunk * prev, * next;
	libsqfs_image_t image;
	libsqfs_chunk_state_t state;
	
	bool may_compress;
	
	libsqfs_data_piece src;
	struct {
		libsqfs_off_t offset;
		size_t size;
		void * data;
		bool compressed;
	} dst;
};
typedef struct _libsqfs_chunk_list {
	libsqfs_chunk * first, * last;
} libsqfs_chunk_list;

void
libsqfs_image_submit_chunk(libsqfs_image_t image, libsqfs_chunk * chunk);

libsqfs_chunk *
libsqfs_image_submit_chunk_for_data(libsqfs_image_t image, libsqfs_data_t data,
	libsqfs_off_t offset, size_t size, bool may_compress);

void
libsqfs_finish_chunks(libsqfs_image_t image);

void
libsqfs_chunk_destroy(libsqfs_chunk * chunk);

#endif
