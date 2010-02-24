#ifndef LIBSQFS_FRAGMENT_H
#define LIBSQFS_FRAGMENT_H

#include <libsqfs.h>
#include "chunks.h"

/* fragments */

typedef struct _libsqfs_fragment_piece libsqfs_fragment_piece;
typedef struct _libsqfs_fragment_block libsqfs_fragment_block;

/* tails of multiple files may be grouped into a single fragment block;
a "piece" describes this composition of a fragment block of multiple
parts, and allows files to identify "their" tail piece */
struct _libsqfs_fragment_piece {
	libsqfs_fragment_piece * prev, * next;
	libsqfs_fragment_block * fragment;
	/* placement within fragment block */
	size_t offset;
	
	/* description of where source data originates from */
	libsqfs_data_piece src;
};

struct _libsqfs_fragment_block {
	libsqfs_fragment_block * prev, * next;
	size_t index, size;
	libsqfs_chunk * chunk;
	
	struct {
		libsqfs_fragment_piece * first, * last;
		size_t count;
	} pieces;
};

typedef struct _libsqfs_fragment_table {
	libsqfs_off_t offset;
	struct {
		libsqfs_fragment_block * first, * last;
		size_t count;
	} fragments;
	
	libsqfs_fragment_block * open_fragment;
} libsqfs_fragment_table;

libsqfs_fragment_piece *
libsqfs_image_submit_fragment_piece(libsqfs_image_t image, libsqfs_data_t data, size_t size, libsqfs_off_t offset);

bool
libsqfs_image_flush_fragments(libsqfs_image_t image);

void
libsqfs_fragment_table_init(libsqfs_fragment_table * frag_table);

void
libsqfs_fragment_table_destroy(libsqfs_fragment_table * frag_table);

bool
libsqfs_fragment_table_write(libsqfs_image_t image, libsqfs_fragment_table * frag_table);

#endif
