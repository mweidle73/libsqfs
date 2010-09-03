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

#ifndef LIBSQFS_XATTRS_H
#define LIBSQFS_XATTRS_H

#include <libsqfs.h>

#include "metatable.h"
#include "compressor.h"

typedef struct _libsqfs_xattr libsqfs_xattr;
typedef struct _libsqfs_xattrset libsqfs_xattrset;

struct _libsqfs_xattr {
	/* all allocated xattrsets for an image chained together */
	libsqfs_xattr_t prev, next;
	
	/* full name (including domain prefix string), e.g. "user.attribute_name" */
	char * name;
	
	/* full name minus domain prefix, e.g. "attribute_name" */
	char * suffix; 
	size_t suffix_length;
	
	/* numeric value corresponding to domain prefix literal, e.g. SQUASHFS_XATTR_USER */
	int domain;
	
	/* opaque xattr data */
	void * value;
	size_t value_length;
};

struct _libsqfs_xattrset {
	/* all allocated xattrsets for an image chained together */
	libsqfs_xattrset_t prev, next;
	
	libsqfs_image_t image;
	
	/* attributes */
	libsqfs_xattr_t * attrs;
	size_t nattrs;
	
	/* xattr id, as referenced by inodes */
	int id;
	
	/* location where xattr data is stored */
	libsqfs_metatable_entry stored_pos;
	
	/* encoded size of xattr data */
	size_t stored_size;
};

typedef struct _libsqfs_xattr_table libsqfs_xattr_table;

struct _libsqfs_xattr_table {
	/* linked list of all xattrs allocated for this image */
	struct {
		libsqfs_xattr_t first, last;
	} xattrs;
	
	/* linked list of all xattrsets allocated for this image */
	struct {
		libsqfs_xattrset_t first, last;
	} xattrsets;
	
	int nids;
	libsqfs_off_t offset;
	
	/* table containing xattr data */
	libsqfs_metatable xattr_data;
	
	/* table containing descriptor handles, pointing to xattr data elements */
	libsqfs_metatable xattr_descriptors;
};

void
libsqfs_xattr_table_init(libsqfs_xattr_table * tab, libsqfs_compressor_instance * compressor);

bool
libsqfs_xattr_table_write(libsqfs_xattr_table * tab, libsqfs_image_t image);

void
libsqfs_xattr_table_fini(libsqfs_xattr_table * tab);

#endif
