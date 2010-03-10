#ifndef LIBSQFS_METATABLE_H
#define LIBSQFS_METATABLE_H

#include <stdbool.h>
#include <stdlib.h>

#include <libsqfs.h>

#include "compressor.h"

#include "squashfs_fs.h"

typedef struct _libsqfs_metablock libsqfs_metablock;
typedef struct _libsqfs_metatable libsqfs_metatable;
typedef struct _libsqfs_metatable_entry libsqfs_metatable_entry;

struct _libsqfs_metablock {
	libsqfs_metablock * prev, * next;
	
	size_t size;
	
	unsigned int offset;
	bool compressed;
	
	char data[SQUASHFS_METADATA_SIZE];
};

struct _libsqfs_metatable {
	/* list of completely filled metablocks */
	libsqfs_metablock * first, * last;
	size_t nmetablocks;
	
	/* metablock currently being filled */
	libsqfs_metablock * opened_block;
	size_t opened_block_fill;
	
	/* compressor to be used */
	libsqfs_compressor_instance * compressor;
	
	/* compressed size of metablocks finished so far (total size
	after "write" has finished) */
	unsigned int size;
	
	libsqfs_off_t offset;
};

struct _libsqfs_metatable_entry {
	/* offset of the (possibly compressed) begin of the block, relative to
	the begin of the table */
	unsigned int block;
	/* offset within compressed block */
	unsigned int offset;
};

void
libsqfs_metatable_init(libsqfs_metatable * tab, libsqfs_compressor_instance * compressor);

void
libsqfs_metatable_fini(libsqfs_metatable * tab);

bool
libsqfs_metatable_append(libsqfs_metatable * tab, const void * data, size_t count, 
libsqfs_metatable_entry * pos);

bool
libsqfs_metatable_write(libsqfs_metatable * tab, libsqfs_image_t image);

bool
libsqfs_metatable_write_with_index(libsqfs_metatable * tab, libsqfs_image_t image);

#endif
