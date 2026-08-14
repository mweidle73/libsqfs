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
#include <unistd.h>

#include "../src/squashfs_fs.h"

static uint16_t
read_le16(const unsigned char *data)
{
	return data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t
read_le32(const unsigned char *data)
{
	return read_le16(data) | ((uint32_t)read_le16(data + 2) << 16);
}

static uint64_t
read_le64(const unsigned char *data)
{
	return read_le32(data) | ((uint64_t)read_le32(data + 4) << 32);
}

struct expected_inode {
	uint16_t type;
	uint16_t mode;
	size_t size;
};

int
main(void)
{
	static const char file_data[] = "x";
	static const char symlink_target[] = "file";
	static const struct expected_inode expected[] = {
		{ SQUASHFS_BLKDEV_TYPE, 0600,
		  sizeof(struct squashfs_dev_inode_header) },
		{ SQUASHFS_CHRDEV_TYPE, 0610,
		  sizeof(struct squashfs_dev_inode_header) },
		{ SQUASHFS_FIFO_TYPE, 0620,
		  sizeof(struct squashfs_ipc_inode_header) },
		{ SQUASHFS_FILE_TYPE, 0640,
		  sizeof(struct squashfs_reg_inode_header) + sizeof(uint32_t) },
		{ SQUASHFS_SYMLINK_TYPE, 0777,
		  sizeof(struct squashfs_symlink_inode_header) +
		  sizeof(symlink_target) - 1 },
		{ SQUASHFS_DIR_TYPE, 0750,
		  sizeof(struct squashfs_dir_inode_header) },
	};
	unsigned char superblock[sizeof(struct squashfs_super_block)];
	unsigned char inode_data[SQUASHFS_METADATA_SIZE];
	unsigned char metadata_header[2];
	char image_name[] = "/tmp/libsqfs-inode-mode-XXXXXX";
	libsqfs_image_options_t options;
	libsqfs_destination_t destination;
	libsqfs_image_t image;
	libsqfs_directory_inode_t root;
	libsqfs_data_t data;
	uint64_t inode_table_start;
	uint16_t inode_data_size;
	size_t offset;
	int fd;

	fd = mkstemp(image_name);
	assert(fd >= 0);
	assert(unlink(image_name) == 0);

	options = libsqfs_image_options_create();
	assert(options);
	libsqfs_image_options_set_inode_compression(options, false);
	libsqfs_image_options_set_fragment_option(options,
		libsqfs_fragments_never);
	libsqfs_image_options_set_padding(options, false);

	destination = libsqfs_destination_create_for_filedes(fd);
	assert(destination);
	image = libsqfs_image_create(destination, options);
	assert(image);
	libsqfs_image_options_destroy(options);

	root = libsqfs_directory_inode_create(image,
		libsqfs_inodeattr_create_simple(image, 0, 0,
			S_IFDIR | 0750, 0));
	assert(root);
	assert(libsqfs_image_set_root(image, root));

	assert(libsqfs_directory_add_entry(root, "block",
		libsqfs_device_inode_downcast(libsqfs_device_inode_create(image,
			libsqfs_inodeattr_create_simple(image, 0, 0,
				S_IFBLK | 0600, 0), 'b', 2, 0))));
	assert(libsqfs_directory_add_entry(root, "char",
		libsqfs_device_inode_downcast(libsqfs_device_inode_create(image,
			libsqfs_inodeattr_create_simple(image, 0, 0,
				S_IFCHR | 0610, 0), 'c', 4, 0))));
	assert(libsqfs_directory_add_entry(root, "fifo",
		libsqfs_fifo_inode_downcast(libsqfs_fifo_inode_create(image,
			libsqfs_inodeattr_create_simple(image, 0, 0,
				S_IFIFO | 0620, 0)))));

	data = libsqfs_data_create_for_static_buffer(image, file_data,
		sizeof(file_data) - 1);
	assert(data);
	assert(libsqfs_directory_add_entry(root, "file",
		libsqfs_regular_inode_downcast(libsqfs_regular_inode_create(image,
			libsqfs_inodeattr_create_simple(image, 0, 0,
				S_IFREG | 0640, 0), data))));
	assert(libsqfs_directory_add_entry(root, "link",
		libsqfs_symlink_inode_downcast(libsqfs_symlink_inode_create(image,
			libsqfs_inodeattr_create_simple(image, 0, 0,
				S_IFLNK | 0777, 0), symlink_target))));

	assert(libsqfs_image_close(image) == libsqfs_image_finalized);

	assert(pread(fd, superblock, sizeof(superblock), 0) ==
		(ssize_t)sizeof(superblock));
	assert(read_le32(superblock) == SQUASHFS_MAGIC);
	inode_table_start = read_le64(superblock + 64);
	assert(pread(fd, metadata_header, sizeof(metadata_header),
		(off_t)inode_table_start) == (ssize_t)sizeof(metadata_header));
	assert(read_le16(metadata_header) & SQUASHFS_COMPRESSED_BIT);
	inode_data_size = read_le16(metadata_header) &
		~SQUASHFS_COMPRESSED_BIT;
	assert(inode_data_size <= sizeof(inode_data));
	assert(pread(fd, inode_data, inode_data_size,
		(off_t)inode_table_start + 2) == inode_data_size);

	offset = 0;
	for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
		assert(offset + expected[i].size <= inode_data_size);
		assert(read_le16(inode_data + offset) == expected[i].type);
		assert(read_le16(inode_data + offset + 2) == expected[i].mode);
		assert((read_le16(inode_data + offset + 2) & S_IFMT) == 0);
		offset += expected[i].size;
	}
	assert(offset == inode_data_size);

	libsqfs_destination_release(destination);
	assert(close(fd) == 0);
	return 0;
}
