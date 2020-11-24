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

#include <zstd.h>

typedef struct libsqfs_compressor_instance_zstd {
	libsqfs_compressor_instance base;
} libsqfs_compressor_instance_zstd;

static void
libsqfs_compressor_instance_zstd_destroy(libsqfs_compressor_instance * self_)
{
	libsqfs_compressor_instance_zstd * self = (libsqfs_compressor_instance_zstd *)self_;
	free(self);
}

static ssize_t
libsqfs_compressor_instance_zstd_compress(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	size_t ret = ZSTD_compress(dst, dst_size, src, src_size, ZSTD_CLEVEL_DEFAULT);

	if (ZSTD_isError(ret)) {
		return -1;
	} else {
		return (ssize_t) ret;
	}
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_zstd_vmt = {
	.destroy = &libsqfs_compressor_instance_zstd_destroy,
	.compress = &libsqfs_compressor_instance_zstd_compress,
	.compress_meta = &libsqfs_compressor_instance_zstd_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_zstd_open(const libsqfs_compressor * self)
{
	(void) self;
	libsqfs_compressor_instance_zstd * instance = malloc(sizeof(*instance));
	if (!instance)
		return 0;
	
	instance->base.vmt = &libsqfs_compressor_instance_zstd_vmt;
	
	return &instance->base;
}

static void
libsqfs_compressor_zstd_destroy(libsqfs_compressor * self)
{
	free(self);
}

static libsqfs_compressor *
libsqfs_compressor_zstd_copy(const libsqfs_compressor * self)
{
	libsqfs_compressor * copy = malloc(sizeof(*copy));
	if (!copy) return 0;
	*copy = *self;
	
	return copy;
}

static libsqfs_compressor_option_data *
libsqfs_compressor_zstd_get_option_data(const libsqfs_compressor * self)
{
	return 0;
}

const libsqfs_compressor libsqfs_compressor_zstd = {
	.destroy = &libsqfs_compressor_zstd_destroy,
	.copy = &libsqfs_compressor_zstd_copy,
	.open = &libsqfs_compressor_zstd_open,
	.get_option_data = &libsqfs_compressor_zstd_get_option_data,
	.id = ZSTD_COMPRESSION
};
