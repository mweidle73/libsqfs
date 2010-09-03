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

#ifndef LIBSQFS_INODES_H
#define LIBSQFS_INODES_H

#include <libsqfs.h>

#include "metatable.h"
#include "compressor.h"

/* inodes */

struct _libsqfs_inodeattr {
	libsqfs_inodeattr_t prev, next;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t ctime;
	
	uint16_t mapped_uid, mapped_gid;
	
	libsqfs_xattrset_t xattrset;
};

void
libsqfs_inodeattr_destroy(libsqfs_inodeattr_t attr);

typedef struct _libsqfs_inode_vmt libsqfs_inode_vmt;

struct _libsqfs_inode_vmt {
	void (*destroy)(libsqfs_inode_t inode);
	bool (*serialize)(libsqfs_inode_t inode);
};

struct _libsqfs_inode {
	const libsqfs_inode_vmt * vmt;
	libsqfs_inode_t prev, next;
	libsqfs_image_t image;
	
	libsqfs_inodeattr_t attr;
	size_t nlink;
	
	libsqfs_metatable_entry inode_table_entry;
	unsigned int inode_number;
	int encoded_type;
};

void
libsqfs_inode_init(libsqfs_image_t image, libsqfs_inode_t inode, libsqfs_inodeattr_t attr);

void
libsqfs_inode_destroy(libsqfs_inode_t inode);

static inline long long
libsqfs_encoded_inode(const libsqfs_inode_t inode)
{
	return inode->inode_table_entry.offset | (((long long)inode->inode_table_entry.block) << 16);
}

typedef struct _libsqfs_directory_entry libsqfs_directory_entry;

struct _libsqfs_directory_inode {
	struct _libsqfs_inode base;
	
	libsqfs_directory_inode_t parent;
	struct {
		libsqfs_directory_entry * first, * last;
	} entries;
	
	size_t indexed_count;
	libsqfs_directory_entry * first_indexed;
	
	/* location of entry list of this directory within directory table */
	libsqfs_metatable_entry dir_table_entry;
	/* size of encoded directory entry listing */
	size_t encoded_entries_size;
};

/* recursively serialize inodes to disk */
bool
libsqfs_inode_serialize(libsqfs_inode_t inode);

#endif
