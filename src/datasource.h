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

#ifndef LIBSQFS_DATASOURCE_H
#define LIBSQFS_DATASOURCE_H

#include <libsqfs.h>

/* data source items */

typedef struct libsqfs_data_vmt libsqfs_data_vmt;
typedef struct libsqfs_data libsqfs_data;
struct libsqfs_data {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
};

/* query size */
libsqfs_off_t
libsqfs_data_get_size(libsqfs_data_t data);

/* raw read */
ssize_t
libsqfs_data_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset);

const char *
libsqfs_data_describe(libsqfs_data_t data);

void
libsqfs_data_destroy(libsqfs_data_t data);

#endif
