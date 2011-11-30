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

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

typedef struct libsqfs_destination_vmt libsqfs_destination_vmt;

struct libsqfs_destination_vmt {
	void (*close)(libsqfs_destination_t destination);
	ssize_t (*pwrite)(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset);
	int (*truncate)(libsqfs_destination_t destination, libsqfs_off_t offset);
};

struct libsqfs_destination {
	const libsqfs_destination_vmt * vmt;
};

void
libsqfs_destination_release(libsqfs_destination_t destination)
{
	destination->vmt->close(destination);
}

ssize_t
libsqfs_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset)
{
	return destination->vmt->pwrite(destination, buffer, size, offset);
}

int
libsqfs_truncate(libsqfs_destination_t destination, libsqfs_off_t offset)
{
	return destination->vmt->truncate(destination, offset);
}

typedef struct libsqfs_destination_file libsqfs_destination_file;
struct libsqfs_destination_file {
	const libsqfs_destination_vmt * vmt;
	int fd;
	bool owns_fd;
};

static void
libsqfs_destination_file_close(libsqfs_destination_t destination)
{
	libsqfs_destination_file * f = (libsqfs_destination_file *) destination;
	if (f->owns_fd) close(f->fd);
	free(f);
}

static ssize_t
libsqfs_destination_file_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset)
{
	libsqfs_destination_file * f = (libsqfs_destination_file *) destination;
	return pwrite(f->fd, buffer, size, (off_t)offset);
}

static int
libsqfs_destination_file_truncate(libsqfs_destination_t destination, libsqfs_off_t offset)
{
	libsqfs_destination_file * f = (libsqfs_destination_file *) destination;
	return ftruncate(f->fd, (off_t)offset);
}

static const libsqfs_destination_vmt libsqfs_destination_file_vmt = {
	.close = &libsqfs_destination_file_close,
	.pwrite = &libsqfs_destination_file_pwrite,
	.truncate = &libsqfs_destination_file_truncate
};

libsqfs_destination_t
libsqfs_destination_create_for_filedes(int fd)
{
	if (ftruncate(fd, 0)) return 0;
	
	libsqfs_destination_file * destination = malloc(sizeof(*destination));
	if (!destination) {
		errno = ENOMEM;
		return 0;
	}
	
	destination->vmt = &libsqfs_destination_file_vmt;
	destination->fd = fd;
	destination->owns_fd = false;
	
	return (libsqfs_destination_t)destination;
}

libsqfs_destination_t
libsqfs_destination_create_for_file(const char * name, mode_t mode)
{
	int fd = open(name, O_CREAT|O_WRONLY
#if defined(O_CLOEXEC)
		|O_CLOEXEC
#endif
		, mode
	);
	if (fd<0) return 0;
	if (ftruncate(fd, 0)) {
		int error = errno;
		close(fd);
		errno = error;
		return 0;
	}
	
	libsqfs_destination_file * destination = malloc(sizeof(*destination));
	if (!destination) {
		close(fd);
		errno = ENOMEM;
		return 0;
	}
	
	destination->vmt = &libsqfs_destination_file_vmt;
	destination->fd = fd;
	destination->owns_fd = true;
	
	return (libsqfs_destination_t)destination;
}

/**
	\brief Create NULL output
	\return @c squashfs destination handle, or NULL on error with errno set appropriately
	
	Creates an output handle that simply discards all data (useful for testing).
*/

static void
libsqfs_destination_null_close(libsqfs_destination_t destination)
{
	free(destination);
}

static ssize_t
libsqfs_destination_null_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset)
{
	return size;
}

static int
libsqfs_destination_null_truncate(libsqfs_destination_t destination, libsqfs_off_t offset)
{
	return 0;
}

static const libsqfs_destination_vmt libsqfs_destination_null_vmt = {
	.close = &libsqfs_destination_null_close,
	.pwrite = &libsqfs_destination_null_pwrite,
	.truncate = &libsqfs_destination_null_truncate
};

libsqfs_destination_t
libsqfs_destination_create_null(void)
{
	libsqfs_destination_t dst = malloc(sizeof(*dst));
	if (!dst) return 0;
	dst->vmt = &libsqfs_destination_null_vmt;
	return dst;
}

