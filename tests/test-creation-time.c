/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2026 libsqfs contributors
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
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static uint32_t
read_le32(const unsigned char *data)
{
	return data[0] | ((uint32_t)data[1] << 8) |
		((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static uint32_t
create_image(bool set_creation_time, uint32_t creation_time)
{
	unsigned char superblock[12];
	char image_name[] = "/tmp/libsqfs-creation-time-XXXXXX";
	libsqfs_image_options_t options;
	libsqfs_destination_t destination;
	libsqfs_directory_inode_t root;
	libsqfs_image_t image;
	int fd;

	fd = mkstemp(image_name);
	assert(fd >= 0);
	assert(unlink(image_name) == 0);

	options = libsqfs_image_options_create();
	assert(options);
	libsqfs_image_options_set_padding(options, false);
	if (set_creation_time)
		libsqfs_image_options_set_creation_time(options, creation_time);

	destination = libsqfs_destination_create_for_filedes(fd);
	assert(destination);
	image = libsqfs_image_create(destination, options);
	assert(image);
	libsqfs_image_options_destroy(options);

	root = libsqfs_directory_inode_create(image,
		libsqfs_inodeattr_create_simple(image, 0, 0,
			S_IFDIR | 0755, 0));
	assert(root);
	assert(libsqfs_image_set_root(image, root));
	assert(libsqfs_image_close(image) == libsqfs_image_finalized);

	assert(pread(fd, superblock, sizeof(superblock), 0) ==
		(ssize_t)sizeof(superblock));
	assert(read_le32(superblock) == UINT32_C(0x73717368));

	libsqfs_destination_release(destination);
	assert(close(fd) == 0);
	return read_le32(superblock + 8);
}

int
main(void)
{
	time_t before;
	time_t after;
	uint32_t fallback;

	before = time(NULL);
	assert(before >= 0 && (uint64_t)before <= UINT32_MAX);
	fallback = create_image(false, 0);
	after = time(NULL);
	assert(after >= before && (uint64_t)after <= UINT32_MAX);
	assert(fallback >= (uint32_t)before);
	assert(fallback <= (uint32_t)after);

	assert(create_image(true, 0) == 0);
	assert(create_image(true, UINT32_MAX) == UINT32_MAX);

	return 0;
}
