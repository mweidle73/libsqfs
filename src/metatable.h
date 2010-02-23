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
	libsqfs_metablock * first, * last;
	size_t nmetablocks;
	
	libsqfs_metablock * opened_block;
	size_t opened_block_fill;
	
	libsqfs_compressor_instance * compressor;
	
	unsigned int size;
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
libsqfs_metatable_destroy(libsqfs_metatable * tab);

bool
libsqfs_metatable_append(libsqfs_metatable * tab, const void * data, size_t count, 
libsqfs_metatable_entry * pos);

libsqfs_off_t
libsqfs_metatable_write(libsqfs_metatable * tab, libsqfs_image_t image);

libsqfs_off_t
libsqfs_write_metatable(libsqfs_image_t image, void * data, size_t size, bool compressed, bool index_tables);

#endif
