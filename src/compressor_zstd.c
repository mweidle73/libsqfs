/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2020
 * secunet Security Networks AG, Markus Theil <markus.theil@secunet.com>
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
#include <stdio.h>
#include <zstd.h>

typedef struct libsqfs_compressor_zstd {
	libsqfs_compressor base;
	uint32_t preset;
} libsqfs_compressor_zstd;

typedef struct libsqfs_compressor_instance_zstd {
	libsqfs_compressor_instance base;
	uint32_t preset;
} libsqfs_compressor_instance_zstd;

static void
libsqfs_compressor_instance_zstd_destroy(libsqfs_compressor_instance * self_)
{
	libsqfs_compressor_instance_zstd * self = (libsqfs_compressor_instance_zstd *)self_;
	free(self);
}

static ssize_t
libsqfs_compressor_instance_zstd_compress(libsqfs_compressor_instance_zstd * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	libsqfs_compressor_instance_zstd * self = (libsqfs_compressor_instance_zstd *)self_;
	size_t ret = ZSTD_compress(dst, dst_size, src, src_size, self->preset);

	if (!ZSTD_isError(ret))
		return (ssize_t)ret;
	else
		return -1;
}

static ssize_t
libsqfs_compressor_instance_zstd_compress_data(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	libsqfs_compressor_instance_zstd * self = (libsqfs_compressor_instance_zstd *)self_;
	return libsqfs_compressor_instance_zstd_compress(self, dst, dst_size, src, src_size);
}

static ssize_t
libsqfs_compressor_instance_zstd_compress_meta(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	libsqfs_compressor_instance_zstd * self = (libsqfs_compressor_instance_zstd *)self_;
	return libsqfs_compressor_instance_zstd_compress(self, dst, dst_size, src, src_size);
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_zstd_vmt = {
	.destroy = &libsqfs_compressor_instance_zstd_destroy,
	.compress = &libsqfs_compressor_instance_zstd_compress_data,
	.compress_meta = &libsqfs_compressor_instance_zstd_compress_meta
};

static libsqfs_compressor_instance *
libsqfs_compressor_zstd_open(const libsqfs_compressor * self_)
{
	const libsqfs_compressor_zstd * self = (const libsqfs_compressor_zstd *) self_;
	libsqfs_compressor_instance_zstd * instance = malloc(sizeof(*instance));
	if (!instance)
		return 0;
	
	instance->base.vmt = &libsqfs_compressor_instance_zstd_vmt;
	instance->preset = self->preset;
	
	return &instance->base;
}

static void
libsqfs_compressor_zstd_destroy(libsqfs_compressor * self_)
{
	libsqfs_compressor_zstd * self = (libsqfs_compressor_zstd *) self_;
	free(self);
}

static libsqfs_compressor *
libsqfs_compressor_zstd_copy(const libsqfs_compressor * self_)
{
	const libsqfs_compressor_zstd * self = (const libsqfs_compressor_zstd *) self_;
	libsqfs_compressor_zstd * copy = malloc(sizeof(*copy));
	if (!copy) return 0;
	*copy = *self;
	
	return &copy->base;
}

static libsqfs_compressor_option_data *
libsqfs_compressor_zstd_get_option_data(const libsqfs_compressor * self_)
{
	return NULL;
}

libsqfs_compressor *
libsqfs_compressor_zstd_create_default(void)
{
	libsqfs_compressor_zstd * self = malloc(sizeof(*self));
	if (!self)
		return 0;
	
	self->base.destroy = &libsqfs_compressor_zstd_destroy;
	self->base.copy = &libsqfs_compressor_zstd_copy;
	self->base.open = &libsqfs_compressor_zstd_open;
	self->base.get_option_data = &libsqfs_compressor_zstd_get_option_data;
	self->base.id = ZSTD_COMPRESSION;
	self->preset = ZSTD_CLEVEL_DEFAULT;
	
	return &self->base;
}

libsqfs_compressor *
libsqfs_compressor_zstd_create_level(int level)
{
	libsqfs_compressor_zstd * self;

	if ((level < 0) || (level > 22)) {
		fprintf(stderr, "zstd: invalid compression level: %d"
			" (allowed 0..22)\n", level);
		return 0;
	}

	self = (libsqfs_compressor_zstd *)libsqfs_compressor_zstd_create_default();
	if (!self)
		return 0;

	self->preset = level;

	return &self->base;
}
