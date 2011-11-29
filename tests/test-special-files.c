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

#include "malloc-verify.h"

int main(void)
{
	malloc_verify_start();
	
	libsqfs_destination_t dest = libsqfs_destination_create_null();
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_inodeattr_t fileattr = libsqfs_inodeattr_create_simple(image, 0, 0, 0600, 0);
	libsqfs_inodeattr_t dirattr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, dirattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_device_inode_t chrdev, blkdev;
	chrdev = libsqfs_device_inode_create(image, fileattr, 'c', 4, 0);
	blkdev = libsqfs_device_inode_create(image, fileattr, 'b', 2, 0);
	libsqfs_symlink_inode_t symlink = libsqfs_symlink_inode_create(image, fileattr, "fd0");
	
	libsqfs_directory_add_entry(rootdir, "tty0", libsqfs_device_inode_downcast(chrdev));
	libsqfs_directory_add_entry(rootdir, "fd0", libsqfs_device_inode_downcast(blkdev));
	libsqfs_directory_add_entry(rootdir, "floppy", libsqfs_symlink_inode_downcast(symlink));
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	malloc_verify_end();
	
	return 0;
}
