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

typedef struct libsqfs_compressor_instance_zlib {
	libsqfs_compressor_instance base;
	z_stream strm;
} libsqfs_compressor_instance_zlib;

static void
libsqfs_compressor_instance_zlib_destroy(libsqfs_compressor_instance * self_)
{
	libsqfs_compressor_instance_zlib * self = (libsqfs_compressor_instance_zlib *)self_;
	deflateEnd(&self->strm);
	free(self);
}

static ssize_t
libsqfs_compressor_instance_zlib_compress(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	ssize_t written = 0;
	libsqfs_compressor_instance_zlib * self = (libsqfs_compressor_instance_zlib *)self_;
	
	self->strm.next_in = (void *)src;
	self->strm.avail_in = src_size;
	self->strm.next_out = dst;
	self->strm.avail_out = dst_size;
	
	deflate(&self->strm, Z_NO_FLUSH);
	int result = deflate(&self->strm, Z_FINISH);
	if (result != Z_STREAM_END)
		written = -1;
	else
		written = self->strm.total_out;
	deflateReset(&self->strm);
	return written;
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_zlib_vmt = {
	.destroy = &libsqfs_compressor_instance_zlib_destroy,
	.compress = &libsqfs_compressor_instance_zlib_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_zlib_open(const libsqfs_compressor * self)
{
	(void) self;
	libsqfs_compressor_instance_zlib * instance = malloc(sizeof(*instance));
	if (!instance) return 0;
	
	instance->base.vmt = &libsqfs_compressor_instance_zlib_vmt;
	instance->strm.zalloc = Z_NULL;
	instance->strm.zfree = Z_NULL;
	if (deflateInit(&instance->strm, 9) != Z_OK) {
		free(instance);
		return 0;
	}
	
	return &instance->base;
}

static void
libsqfs_compressor_zlib_destroy(libsqfs_compressor * self)
{
	free(self);
}

static libsqfs_compressor *
libsqfs_compressor_zlib_copy(const libsqfs_compressor * self)
{
	libsqfs_compressor * copy = malloc(sizeof(*copy));
	if (!copy) return 0;
	*copy = *self;
	
	return copy;
}

const libsqfs_compressor libsqfs_compressor_zlib = {
	.destroy = &libsqfs_compressor_zlib_destroy,
	.copy = &libsqfs_compressor_zlib_copy,
	.open = &libsqfs_compressor_zlib_open,
	.id = ZLIB_COMPRESSION
};
