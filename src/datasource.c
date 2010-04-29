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
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

struct _libsqfs_data_vmt {
	libsqfs_off_t (*get_size)(libsqfs_data_t data);
	ssize_t (*pread)(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset);
	const char * (*describe)(libsqfs_data_t data);
	void (*destroy)(libsqfs_data_t data);
};

libsqfs_off_t
libsqfs_data_get_size(libsqfs_data_t data)
{
	return data->vmt->get_size(data);
}

ssize_t
libsqfs_data_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	return data->vmt->pread(data, buffer, size, offset);
}

const char *
libsqfs_data_describe(libsqfs_data_t data)
{
	return data->vmt->describe(data);
}

void
libsqfs_data_destroy(libsqfs_data_t data)
{
	data->vmt->destroy(data);
}

static void
libsqfs_data_init(libsqfs_image_t image, libsqfs_data_t data)
{
	data->image = image;
	data->prev = image->dataitems.last;
	data->next = 0;
	if (image->dataitems.last) image->dataitems.last->next = data;
	else image->dataitems.first = data;
	image->dataitems.last = data;
}

typedef struct _libsqfs_filedata {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
	char * pathname;
} libsqfs_filedata;

static libsqfs_off_t
libsqfs_filedata_get_size(libsqfs_data_t data)
{
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	struct stat st;
	int error = stat(filedata->pathname, &st);
	if (error) {
		char errormsg[1024];
		snprintf(errormsg, sizeof(errormsg), "Unable to stat file %s: %s",
			filedata->pathname, strerror(errno));
		libsqfs_image_flag_error(data->image, errormsg, false);
		return -1;
	}
	else return st.st_size;
}

static ssize_t
libsqfs_filedata_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	/* currently, a new file descriptor is opened for every operation;
	this obviuosly sucks, but for now it is good enough */
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	int fd = open(filedata->pathname, O_RDONLY);
	if (fd<0) return -1;
	
	ssize_t count = pread(fd, buffer, size, (off_t)offset);
	if (count != size) {
		/* "close" might conceivably change errno, so preserve it */
		int saved_errno = errno;
		close(fd);
		errno = saved_errno;
		return count;
	}
	close(fd);
	
	return count;
}

static const char *
libsqfs_filedata_describe(libsqfs_data_t data)
{
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	return filedata->pathname;
}

static void
libsqfs_filedata_destroy(libsqfs_data_t data)
{
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	free(filedata->pathname);
	free(filedata);
}

const libsqfs_data_vmt libsqfs_filedata_vmt = {
	.get_size = &libsqfs_filedata_get_size,
	.pread = &libsqfs_filedata_pread,
	.describe = &libsqfs_filedata_describe,
	.destroy = &libsqfs_filedata_destroy
};

libsqfs_data_t
libsqfs_data_create_from_file(libsqfs_image_t image, const char * srcpath)
{
	libsqfs_filedata * data = malloc(sizeof(*data));
	if (!data) return 0;
	
	data->pathname = strdup(srcpath);
	if (!data->pathname) {
		libsqfs_image_out_of_memory(image, false);
		free(data);
		return 0;
	}
	
	libsqfs_data_init(image, (libsqfs_data_t)data);
	
	data->vmt = &libsqfs_filedata_vmt;
	
	return (libsqfs_data_t)data;
}

typedef struct _libsqfs_memorydata {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
	const void * buffer;
	size_t size;
	
	void (*deleter)(void * closure);
	void * deleter_closure;
} libsqfs_memorydata;

static libsqfs_off_t
libsqfs_memorydata_get_size(libsqfs_data_t data)
{
	libsqfs_memorydata * memorydata = (libsqfs_memorydata *)data;
	return memorydata->size;
}

static ssize_t
libsqfs_memorydata_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	libsqfs_memorydata * memorydata = (libsqfs_memorydata *)data;
	if (offset > memorydata->size) return 0;
	if (offset+size > memorydata->size) size = memorydata->size - offset;
	memcpy(buffer, offset + (const char *)memorydata->buffer, size);
	return size;
}

static const char *
libsqfs_memorydata_describe(libsqfs_data_t data)
{
	return "<memory-block>";
}

static void
libsqfs_memorydata_destroy(libsqfs_data_t data)
{
	libsqfs_memorydata * memorydata = (libsqfs_memorydata *)data;
	if (memorydata->deleter) memorydata->deleter(memorydata->deleter_closure);
	
	free(memorydata);
}

const libsqfs_data_vmt libsqfs_memorydata_vmt = {
	.get_size = &libsqfs_memorydata_get_size,
	.pread = &libsqfs_memorydata_pread,
	.describe = &libsqfs_memorydata_describe,
	.destroy = &libsqfs_memorydata_destroy
};

libsqfs_data_t
libsqfs_data_create_for_buffer(libsqfs_image_t image, const void * buffer, size_t size, void (*deleter)(void *), void * deleter_closure)
{
	libsqfs_memorydata * data = malloc(sizeof(*data));
	if (!data) return 0;
	
	data->buffer = buffer;
	data->size = size;
	data->deleter = deleter;
	data->deleter_closure = deleter_closure;
	
	libsqfs_data_init(image, (libsqfs_data_t)data);
	data->vmt = &libsqfs_memorydata_vmt;
	
	return (libsqfs_data_t)data;
}

libsqfs_data_t
libsqfs_data_create_for_static_buffer(libsqfs_image_t image, const void * buffer, size_t size)
{
	return libsqfs_data_create_for_buffer(image, buffer, size, 0, 0);
}

libsqfs_data_t
libsqfs_data_create_for_transferred_buffer(libsqfs_image_t image, void * buffer, size_t size)
{
	return libsqfs_data_create_for_buffer(image, buffer, size, free, buffer);
}

typedef struct _libsqfs_compounddata {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
	
	libsqfs_data_piece * pieces;
	size_t npieces;
	
	libsqfs_off_t size;
} libsqfs_compounddata;

static libsqfs_off_t
libsqfs_compounddata_get_size(libsqfs_data_t data)
{
	libsqfs_compounddata * comp = (libsqfs_compounddata *)data;
	return comp->size;
}

static ssize_t
libsqfs_compounddata_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	libsqfs_compounddata * comp = (libsqfs_compounddata *)data;
	if (offset > comp->size) return 0;
	if (offset+size > comp->size) size = comp->size - offset;
	
	/* for each piece, check if it overlaps with the range to be read */
	ssize_t written = 0;
	size_t n;
	libsqfs_off_t current_offset = 0;
	for(n=0; n<comp->npieces; n++) {
		libsqfs_off_t piece_begin = current_offset;
		libsqfs_off_t piece_end = current_offset + comp->pieces[n].size;
		
		current_offset = piece_end;
		if (piece_end <= offset) continue;
		if (piece_begin >= offset+size) break;
		
		/* copy data in overlapping range */
		libsqfs_off_t copy_begin = piece_begin, copy_end = piece_end;
		if (copy_begin < offset) copy_begin = offset;
		if (copy_end > offset+size) copy_end = offset+size;
		
		ssize_t count = libsqfs_data_pread(
			comp->pieces[n].data,
			(copy_begin-offset) + (char *) buffer,
			copy_end - copy_begin,
			comp->pieces[n].offset + copy_begin - piece_begin);
		if (count<0) break;
		written += count;
		if (count != copy_end - copy_begin) break;
	}
	return written;
}

static const char *
libsqfs_compounddata_describe(libsqfs_data_t data)
{
	return "<compound-data-block>";
}


static void
libsqfs_compounddata_destroy(libsqfs_data_t data)
{
	libsqfs_compounddata * comp = (libsqfs_compounddata *)data;
	free(comp->pieces);
	free(comp);
}

const libsqfs_data_vmt libsqfs_compounddata_vmt = {
	.get_size = &libsqfs_compounddata_get_size,
	.pread = &libsqfs_compounddata_pread,
	.describe = &libsqfs_compounddata_describe,
	.destroy = &libsqfs_compounddata_destroy
};

libsqfs_data_t
libsqfs_data_create_compound(libsqfs_image_t image, size_t npieces,
	const libsqfs_data_piece pieces[])
{
	libsqfs_compounddata * comp = malloc(sizeof(*comp));
	if (!comp) return 0;
	
	comp->pieces = malloc(sizeof(pieces[0]) * npieces);
	if (!comp->pieces) {
		free(comp);
		return 0;
	}
	
	comp->npieces = npieces;
	comp->size = 0;
	size_t n;
	for(n=0; n<npieces; n++) {
		comp->pieces[n] = pieces[n];
		comp->size += pieces[n].size;
	}
	
	libsqfs_data_init(image, (libsqfs_data_t)comp);
	
	comp->vmt = &libsqfs_compounddata_vmt;
	
	return (libsqfs_data_t) comp;
}
