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

#include "internal.h"
#include "squashfs_fs.h"

void
libsqfs_export_table_init(libsqfs_export_table * export_tab)
{
	export_tab->offset = -1;
}

bool
libsqfs_export_table_write(libsqfs_image_t image, libsqfs_export_table * export_tab)
{
	uint64_t inode_map[image->inodes.count];
	
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		inode_map[inode->inode_number-1] = cpu_to_le64(libsqfs_encoded_inode(inode));
		inode = inode->next;
	}
	
	libsqfs_metatable tab;
	libsqfs_metatable_init(&tab, 0);
	
	bool success = libsqfs_metatable_append(&tab, inode_map, sizeof(inode_map), 0);
	success = success && libsqfs_metatable_write_with_index(&tab, image);
	
	export_tab->offset = tab.offset;
	
	libsqfs_metatable_fini(&tab);
	
	return success;
}
