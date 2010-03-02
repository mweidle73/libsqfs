#ifndef LIBSQFS_BULKDATA_H
#define LIBSQFS_BULKDATA_H

#include <libsqfs.h>
#include "threadpool.h"
#include "compressor.h"

/* "bulk data" is any non-metadata, i.e. the actual content of files,
extended attributes etc. Since this makes up the majority of data within
a squashfs image, this attempts to aggressively parallelize the processing
of the chunks.

There is one important constraint imposed by the squashfs format, though:
All blocks comprising the data of a single file must be stored contiguously.
One other requirement (not imposed by the format, though), is the desire
to produce "consistent" images, which means that out-of-order processing
of data elements should not result in random placements of data in the
final image (so two runs using identical input data produce identical images).

The approach taken here is to allow arbitrary out-of-order processing, but
serialize on write-out: Each thread finishing one data item checks if it
can be written out: If it is unsuccessful, it just leaves write-out
pending for another thread to clean up afterwards. If it is successful,
it will continue writing out deferred items left over by other threads.
*/

typedef struct _libsqfs_chunk libsqfs_chunk;
typedef struct _libsqfs_fragment_piece libsqfs_fragment_piece;
typedef struct _libsqfs_fragment_block libsqfs_fragment_block;
typedef struct _libsqfs_full_block libsqfs_full_block;
typedef struct _libsqfs_image_block libsqfs_image_block;
typedef struct _libsqfs_bulkdata libsqfs_bulkdata;

/* A "chunk" is a generic piece of bulk data, to be written to the squashfs
image eventually; chunks come in two flavors:

- "full blocks": blocks comprising data from a single file, simply
  read, compressed and stored
- "fragment pieces": pieces of files that are too small to form a full
  block, they are packed together up to the size of a full block,
  compressed & stored together

Chunks are distributed to multiple threads for processing, they may thus
be processed "out of order".

Chunks will be "written out" to the image in the order they were enqueued,
but "fragment pieces" are a slight exception: When processing a fragment piece,
it will be attempted to place the piece into the most recently "opened" fragment
block. If this is not possible (because this placement would "overflow" the block),
the most recent fragment block is written to the image in this place instead, and
a new fragment block is opened. */

typedef enum {
	/* chunk has not been processed at all */
	libsqfs_chunk_none = 0,
	/* chunk has been read into memory */
	libsqfs_chunk_read = 1,
	/* chunk has gone through deduplication */
	libsqfs_chunk_deduplicated = 2,
	/* chunk has been assigned to an image block */
	libsqfs_chunk_assigned = 3,
	/* chunk has been compressed */
	libsqfs_chunk_writable = 4,
	/* chunk has been written to disk */
	libsqfs_chunk_finished = 5
} libsqfs_chunk_state_t;

typedef struct _libsqfs_chunk_vmt libsqfs_chunk_vmt;

struct _libsqfs_image_block {
	libsqfs_off_t offset;
	size_t size;
	void * data;
	bool compressed;
};

#define LIBSQFS_CHUNK_COMMON \
	const libsqfs_chunk_vmt * vmt; \
	libsqfs_bulkdata * bulkdata; \
	\
	/* chain of chunks in the order they must be stored in the image */ \
	libsqfs_chunk * prev, * next; \
	\
	/* processing state */ \
	libsqfs_chunk * workq_prev, * workq_next; \
	libsqfs_chunk_state_t state; \
	bool claimed_by_thread; \
	\
	/* description of where source data originates from */ \
	libsqfs_data_piece data; \
	\
	/* possibly cached source data read from disk */ \
	void * cached_data; \
	\
	/* index, to quickly determine order of arbitrary chunks */ \
	size_t index; \

struct _libsqfs_chunk {
	LIBSQFS_CHUNK_COMMON
};

struct _libsqfs_full_block {
	LIBSQFS_CHUNK_COMMON
	
	/* location where block has been written to disk */
	libsqfs_image_block dst;
};

struct _libsqfs_fragment_piece {
	LIBSQFS_CHUNK_COMMON
	
	libsqfs_fragment_piece * prev_piece, * next_piece;
	
	/* placement within fragment block */
	size_t offset;
	libsqfs_fragment_block * fragment_block;
	libsqfs_fragment_piece * prev_in_block, * next_in_block;
	
	/* previous fragment block must be flushed to image at this location */
	bool flush_previous_block;
};

struct _libsqfs_fragment_block {
	libsqfs_fragment_block * prev, * next;
	size_t index, size;
	
	struct { libsqfs_fragment_piece * first, * last; } pieces;
	
	libsqfs_image_block dst;
};

typedef struct _libsqfs_chunk_workq {
	libsqfs_chunk * first, * last;
} libsqfs_chunk_workq;

struct _libsqfs_bulkdata {
	struct { libsqfs_chunk * first, * last; size_t count, finished; } chunks;
	struct { libsqfs_fragment_block * first, * last; size_t count; } fragment_blocks;
	struct { libsqfs_fragment_piece * first, * last; } fragment_pieces;
	
	libsqfs_chunk_workq readq, dedupq, assignq, compressq, writeq;
	
	/* indicates that new chunks may still be added, i.e. worker threads
	may nwill added, i.e. the image is finished */
	bool may_add_chunks;
	
	/* lock enforcing ordered write-out of bulk data */
	pthread_mutex_t lock;
	pthread_cond_t cond;
	
	/* location of fragment table */
	libsqfs_off_t frag_table_loc;
};

void
libsqfs_bulkdata_init(libsqfs_bulkdata * bd);

void
libsqfs_bulkdata_fini(libsqfs_bulkdata * bd);

libsqfs_full_block *
libsqfs_bulkdata_sumbit(libsqfs_bulkdata * bd, libsqfs_data_piece data);

libsqfs_fragment_piece *
libsqfs_bulkdata_submit_fragment(libsqfs_bulkdata * bd, libsqfs_data_piece data);

void
libsqfs_bulkdata_process(libsqfs_bulkdata * bd, libsqfs_compressor_instance * ci, libsqfs_image_t image);

/* "seals" the image: no more data may be added afterwards; helper
threads will no longer wait for new data to appear; may return
"false" if some error occured while sealing image */
bool
libsqfs_bulkdata_seal(libsqfs_bulkdata * bd);

/* synchronously wait until all bulk data has been written; the
calling thread actively helps in writing image */
bool
libsqfs_bulkdata_finish(libsqfs_bulkdata * bd, libsqfs_image_t image);

/* create and write fragment table */
bool
libsqfs_bulkdata_write_fragment_table(libsqfs_bulkdata * bd, libsqfs_image_t image);

#endif
