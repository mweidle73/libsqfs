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
	
	libsqfs_inodeattr_t iattr = libsqfs_inodeattr_create_simple(image, /* uid */ 1000, /* gid */ 1000, 0755, 0);
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, iattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	malloc_verify_end();
	
	return 0;
}
