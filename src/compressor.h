/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2010
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Refer to the file "COPYING" for details.
 */

#ifndef LIBSQFS_COMPRESSOR_H
#define LIBSQFS_COMPRESSOR_H

#include <stdlib.h>
#include <libsqfs.h>

/* compressor */

typedef struct _libsqfs_compressor_instance libsqfs_compressor_instance;

struct _libsqfs_compressor {
	libsqfs_compressor_instance * (*open)(const libsqfs_compressor * self);
	int id;
};

typedef struct _libsqfs_compressor_instance_vmt {
	void (*destroy)(libsqfs_compressor_instance * i);
	ssize_t (*compress)(libsqfs_compressor_instance * i,
		void * dst, size_t dst_size, const void * src, size_t src_size);
} libsqfs_compressor_instance_vmt;

struct _libsqfs_compressor_instance {
	const libsqfs_compressor_instance_vmt * vmt;
};

libsqfs_compressor_instance *
libsqfs_compressor_open(const libsqfs_compressor * compr);

/* transforms the given input data and returns the number of bytes of
the output area used; if the block could not be compressed (e.g. because
the compressed representation turns out to be larger than the provided
buffer), (size_t)-1 is returned */
ssize_t
libsqfs_compressor_instance_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size);

void
libsqfs_compressor_instance_destroy(libsqfs_compressor_instance * i);

#endif
