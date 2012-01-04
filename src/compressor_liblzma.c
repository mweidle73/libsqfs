/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2011
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

#include <lzma.h>

typedef struct libsqfs_compressor_instance_lzma {
	libsqfs_compressor_instance base;
} libsqfs_compressor_instance_lzma;

static void
libsqfs_compressor_instance_lzma_destroy(libsqfs_compressor_instance * self_)
{
	libsqfs_compressor_instance_lzma * self = (libsqfs_compressor_instance_lzma *)self_;
	free(self);
}

static ssize_t
libsqfs_compressor_instance_lzma_compress(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	ssize_t written = 0;
	/*libsqfs_compressor_instance_lzma * self = (libsqfs_compressor_instance_lzma *)self_;*/
	
	lzma_options_lzma options;
	lzma_lzma_preset(&options, 5 /* magic from mksquashfs */);
	options.dict_size = dst_size;
	
	lzma_stream stream = LZMA_STREAM_INIT;
	int res = lzma_alone_encoder(&stream, &options);
	if (res != LZMA_OK) {
		lzma_end(&stream);
		return -1;
	}
	
	stream.next_in = (void *)src;
	stream.avail_in = src_size;
	stream.next_out = dst;
	stream.avail_out = dst_size;
	
	res = lzma_code(&stream, LZMA_FINISH);
	
	if (res == LZMA_STREAM_END) {
		written = stream.total_out;
		/* fill in 8 byte little endian header */
		uint64_t size_le64 = cpu_to_le64(src_size);
		memcpy(5 + (char *) dst, &size_le64, sizeof(size_le64));
	} else if (res == LZMA_OK) {
		written = -1;
	}
	
	lzma_end(&stream);
	
	return written;
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_lzma_vmt = {
	.destroy = &libsqfs_compressor_instance_lzma_destroy,
	.compress = &libsqfs_compressor_instance_lzma_compress,
	.compress_meta = &libsqfs_compressor_instance_lzma_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_lzma_open(const libsqfs_compressor * self)
{
	(void) self;
	libsqfs_compressor_instance_lzma * instance = malloc(sizeof(*instance));
	if (!instance)
		return 0;
	
	instance->base.vmt = &libsqfs_compressor_instance_lzma_vmt;
	
	return &instance->base;
}

static void
libsqfs_compressor_lzma_destroy(libsqfs_compressor * self)
{
	free(self);
}

static libsqfs_compressor *
libsqfs_compressor_lzma_copy(const libsqfs_compressor * self)
{
	libsqfs_compressor * copy = malloc(sizeof(*copy));
	if (!copy) return 0;
	*copy = *self;
	
	return copy;
}

static libsqfs_compressor_option_data *
libsqfs_compressor_lzma_get_option_data(const libsqfs_compressor * self)
{
	return 0;
}

const libsqfs_compressor libsqfs_compressor_lzma = {
	.destroy = &libsqfs_compressor_lzma_destroy,
	.copy = &libsqfs_compressor_lzma_copy,
	.open = &libsqfs_compressor_lzma_open,
	.get_option_data = &libsqfs_compressor_lzma_get_option_data,
	.id = LZMA_COMPRESSION
};
