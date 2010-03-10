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

#include "internal.h"

#include <zlib.h>

typedef struct _libsqfs_compressor_instance_zlib {
	const libsqfs_compressor_instance_vmt * vmt;
	z_stream strm;
} libsqfs_compressor_instance_zlib;

static void
libsqfs_compressor_instance_zlib_destroy(libsqfs_compressor_instance * i)
{
	libsqfs_compressor_instance_zlib * zi = (libsqfs_compressor_instance_zlib *)i;
	deflateEnd(&zi->strm);
	free(zi);
}

static ssize_t
libsqfs_compressor_instance_zlib_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	ssize_t written = 0;
	libsqfs_compressor_instance_zlib * zi = (libsqfs_compressor_instance_zlib *)i;
	
	zi->strm.next_in = (void *)src;
	zi->strm.avail_in = src_size;
	zi->strm.next_out = dst;
	zi->strm.avail_out = dst_size;
	
	deflate(&zi->strm, Z_NO_FLUSH);
	int result = deflate(&zi->strm, Z_FINISH);
	if (result != Z_STREAM_END)
		written = -1;
	else
		written = zi->strm.total_out;
	deflateReset(&zi->strm);
	return written;
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_zlib_vmt = {
	.destroy = &libsqfs_compressor_instance_zlib_destroy,
	.compress = &libsqfs_compressor_instance_zlib_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_zlib_open(void)
{
	libsqfs_compressor_instance_zlib * zi = malloc(sizeof(*zi));
	if (!zi) return 0;
	
	zi->vmt = &libsqfs_compressor_instance_zlib_vmt;
	zi->strm.zalloc = Z_NULL;
	zi->strm.zfree = Z_NULL;
	if (deflateInit(&zi->strm, 9) != Z_OK) {
		free(zi);
		return 0;
	}
	
	return (libsqfs_compressor_instance *) zi;
}

const libsqfs_compressor libsqfs_compressor_zlib = {
	.open = &libsqfs_compressor_zlib_open,
	.id = 1
};
