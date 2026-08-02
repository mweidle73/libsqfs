/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2026 secunet Security Networks AG
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * Refer to the file "COPYING" for details.
 */

#include <libsqfs.h>

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#ifndef LIBSQFS_HAVE_COMPRESSOR_ZSTD
#error "libzstd was found but the public feature macro is missing"
#endif

static uint16_t
read_compression_id(const char * path)
{
	unsigned char value[2];
	FILE * file = fopen(path, "rb");

	assert(file);
	assert(fseek(file, 20, SEEK_SET) == 0);
	assert(fread(value, sizeof(value), 1, file) == 1);
	assert(fclose(file) == 0);

	return (uint16_t) value[0] | ((uint16_t) value[1] << 8);
}

int
main(void)
{
	libsqfs_compressor_t compressor;
	libsqfs_destination_t destination;
	libsqfs_directory_inode_t root;
	libsqfs_image_options_t options;
	libsqfs_inodeattr_t attributes;
	libsqfs_image_t image;

	compressor = libsqfs_compressor_zstd_create_default();
	assert(compressor);
	libsqfs_compressor_destroy(compressor);

	compressor = libsqfs_compressor_zstd_create_level(0);
	assert(compressor);
	libsqfs_compressor_destroy(compressor);

	compressor = libsqfs_compressor_zstd_create_level(22);
	assert(compressor);
	libsqfs_compressor_destroy(compressor);

	assert(!libsqfs_compressor_zstd_create_level(-1));
	assert(!libsqfs_compressor_zstd_create_level(23));

	options = libsqfs_image_options_create();
	assert(options);
	compressor = libsqfs_compressor_zstd_create_level(3);
	assert(compressor);
	libsqfs_image_options_set_compressor(options, compressor);
	libsqfs_compressor_destroy(compressor);

	destination = libsqfs_destination_create_for_file("zstd-image", 0600);
	assert(destination);
	image = libsqfs_image_create(destination, options);
	assert(image);
	libsqfs_image_options_destroy(options);

	attributes = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	assert(attributes);
	root = libsqfs_directory_inode_create(image, attributes);
	assert(root);
	libsqfs_image_set_root(image, root);
	libsqfs_image_close(image);
	libsqfs_destination_release(destination);

	/* The SquashFS 4 superblock stores its compressor at byte offset 20. */
	assert(read_compression_id("zstd-image") == 6);
	assert(unlink("zstd-image") == 0);

	return 0;
}
