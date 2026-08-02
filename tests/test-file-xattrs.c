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

#include <libsqfs.h>
#include <assert.h>

const char sentence[] = "The quick brown fox jumps over the lazy dog.\n";

int main(void)
{
	libsqfs_destination_t dest = libsqfs_destination_create_null();
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_xattr_t xattr = libsqfs_xattr_create(image, "user.data", sizeof(sentence)-1, sentence);
	libsqfs_xattrset_t xattrset = libsqfs_xattrset_create(image, 1, &xattr);
	assert(xattr);
	
	libsqfs_inodeattr_t fileattr = libsqfs_inodeattr_create_extended(image, 1000, 1000, 0644, 0, xattrset);
	assert(fileattr);
	libsqfs_data_t data = libsqfs_data_create_for_static_buffer(image, sentence, sizeof(sentence)-1);
	libsqfs_regular_inode_t file = libsqfs_regular_inode_create(image, fileattr, data);
	
	libsqfs_inodeattr_t dirattr = libsqfs_inodeattr_create_simple(image, /* uid */ 1000, /* gid */ 1000, 0755, 0);
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, dirattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_directory_add_entry(rootdir, "sentence.txt", libsqfs_regular_inode_downcast(file));
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
