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

#include <assert.h>
#include <stdio.h>
#include <strings.h>
#include <lzma.h>

/* on-disk layout of compressor options */
typedef struct libsqfs_compressor_xz_option_data {
	uint32_t dictionary_size;
	uint32_t flags;
} libsqfs_compressor_xz_option_data;

typedef struct libsqfs_compressor_xz {
	libsqfs_compressor base;
	int dictionary_size;
	int flags;
} libsqfs_compressor_xz;

typedef struct libsqfs_compressor_instance_xz {
	libsqfs_compressor_instance base;
	lzma_filter filters[4];
	lzma_options_lzma lzma_options;
	int dictionary_size;
	int flags;
} libsqfs_compressor_instance_xz;

static void
libsqfs_compressor_instance_xz_destroy(libsqfs_compressor_instance * self_)
{
	libsqfs_compressor_instance_xz * self = (libsqfs_compressor_instance_xz *)self_;
	free(self);
}

static ssize_t
libsqfs_compressor_instance_xz_compress(libsqfs_compressor_instance * self_,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	libsqfs_compressor_instance_xz * self = (libsqfs_compressor_instance_xz *)self_;
	
	size_t ret_dst_size = 0;
	lzma_ret res = lzma_stream_buffer_encode(
		self->filters, LZMA_CHECK_CRC32, NULL,
		(const uint8_t *) src, src_size,
		(uint8_t *) dst, &ret_dst_size, dst_size);
	
	if (res == LZMA_OK) {
		return ret_dst_size;
	} else {
		return -1;
	}
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_xz_vmt = {
	.destroy = &libsqfs_compressor_instance_xz_destroy,
	.compress = &libsqfs_compressor_instance_xz_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_xz_open(const libsqfs_compressor * self_)
{
	const libsqfs_compressor_xz * self = (const libsqfs_compressor_xz *) self_;
	libsqfs_compressor_instance_xz * instance = malloc(sizeof(*instance));
	if (!instance)
		return 0;
	
	instance->base.vmt = &libsqfs_compressor_instance_xz_vmt;
	instance->dictionary_size = self->dictionary_size;
	instance->flags = self->flags;
	
	size_t n = 0;
	
	lzma_lzma_preset(&instance->lzma_options, LZMA_PRESET_DEFAULT);
	instance->filters[n].id = LZMA_FILTER_LZMA2;
	instance->filters[n].options = &instance->lzma_options;
	
	n++;
	instance->filters[n].id = LZMA_VLI_UNKNOWN;
	instance->filters[n].options = NULL;
	
	return &instance->base;
}

static void
libsqfs_compressor_xz_destroy(libsqfs_compressor * self_)
{
	libsqfs_compressor_xz * self = (libsqfs_compressor_xz *) self_;
	free(self);
}

static libsqfs_compressor *
libsqfs_compressor_xz_copy(const libsqfs_compressor * self_)
{
	const libsqfs_compressor_xz * self = (const libsqfs_compressor_xz *) self_;
	libsqfs_compressor_xz * copy = malloc(sizeof(*copy));
	if (!copy) return 0;
	*copy = *self;
	
	return &copy->base;
}

static inline bool
valid_dictionary_size(size_t size)
{
	if (size == 0)
		return true;
	
	/* checks for size are taken from mksquashfs */
	/* 8192 is a minimum, 1048576 is the max. block size */
	if ((size < 8192) || (size > 1048576))
		return false;
	
	/* 2^n and 2^n+2^(n+1) */
	int n = ffs(size) - 1;
	return size == (1 << n) || size == (3 << n);
}

static libsqfs_compressor_option_data *
libsqfs_compressor_xz_get_option_data(const libsqfs_compressor * self_)
{
	const libsqfs_compressor_xz * self = (const libsqfs_compressor_xz *) self_;
	
	/* Atm. changing flags is not supported, so if it is set it might
	 * be messed up */
	assert(self->flags == 0);
	/* more sanity checks */
	assert(valid_dictionary_size(self->dictionary_size));
	
	if (self->dictionary_size == 0 && self->flags == 0)
		return 0;
	
	libsqfs_compressor_xz_option_data * data = malloc(sizeof(*data));
	if (!data)
		return 0;
	
	libsqfs_compressor_option_data * opts = malloc(sizeof(*opts));
	if (!opts) {
		free(data);
		return 0;
	}
	
	data->dictionary_size = cpu_to_le32(self->dictionary_size);
	data->flags = cpu_to_le32(self->flags);
	opts->data = data;
	opts->size = sizeof(*data);
	return opts;
}

libsqfs_compressor *
libsqfs_compressor_xz_create_default(void)
{
	libsqfs_compressor_xz * self = malloc(sizeof(*self));
	if (!self)
		return 0;
	
	self->base.destroy = &libsqfs_compressor_xz_destroy;
	self->base.copy = &libsqfs_compressor_xz_copy;
	self->base.open = &libsqfs_compressor_xz_open;
	self->base.get_option_data = &libsqfs_compressor_xz_get_option_data;
	self->base.id = XZ_COMPRESSION;
	self->dictionary_size = 0;
	self->flags = 0;
	
	return &self->base;
}
