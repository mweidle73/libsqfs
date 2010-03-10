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

#ifndef LIBSQFS_INTERNAL_H
#define LIBSQFS_INTERNAL_H

#include <libsqfs.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "squashfs_fs.h"

#include "compressor.h"
#include "metatable.h"
#include "inodes.h"
#include "datasource.h"
#include "threadpool.h"
#include "bulkdata.h"

/* destinations */

ssize_t
libsqfs_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset);

void
libsqfs_truncate(libsqfs_destination_t destination, libsqfs_off_t offset);

/* entry function for worker threads */
void
libsqfs_image_process_chunks(libsqfs_image_t image);

/* id table  */

typedef struct _libsqfs_idtable libsqfs_idtable;
struct _libsqfs_idtable {
	uint32_t * ids;
	size_t nids;
	libsqfs_off_t offset;
};

void
libsqfs_idtable_init(libsqfs_idtable * idtable);

bool
libsqfs_idtable_write(libsqfs_image_t image, libsqfs_idtable * idtable);

void
libsqfs_idtable_destroy(libsqfs_idtable * idtable);

/* returns 16-bit mapped id, or -1 on mapping failure */
int
libsqfs_idtable_map(libsqfs_idtable * idtable, uint32_t id);

/* export table */

typedef struct _libsqfs_export_table {
	libsqfs_off_t offset;
} libsqfs_export_table;

void
libsqfs_export_table_init(libsqfs_export_table * export_tab);

bool
libsqfs_export_table_write(libsqfs_image_t image, libsqfs_export_table * export_tab);

/* images */

typedef struct _libsqfs_image_options libsqfs_image_options;
struct _libsqfs_image_options {
	bool inode_compression;
	bool data_compression;
	bool fragment_compression;
	libsqfs_fragments_option fragments;
	bool exportable;
	const libsqfs_compressor * compressor;
	bool padding;
	size_t block_size, block_size_log;
};

struct _libsqfs_image {
	libsqfs_image_options options;
	libsqfs_destination_t dst;
	libsqfs_off_t size;
	
	libsqfs_image_state_t state;
	const char * error_msg;
	pthread_mutex_t state_mutex;
	
	uint32_t creation_time;
	
	struct {
		libsqfs_data_t first, last;
	} dataitems;
	struct {
		libsqfs_inodeattr_t first, last;
	} inodeattrs;
	struct {
		libsqfs_inode_t first, last;
		size_t count;
	} inodes;
	
	libsqfs_bulkdata bulkdata;
	libsqfs_idtable idtable;
	libsqfs_metatable inode_table;
	libsqfs_metatable dir_table;
	libsqfs_export_table export_table;
	
	libsqfs_compressor_instance * compressor;
	
	libsqfs_directory_inode_t root;
	libsqfs_threadpool threadpool;
	
};

/* reserve space in image */
libsqfs_off_t
libsqfs_image_reserve(libsqfs_image_t image, size_t bytes);

/* flag error state on image: some memory allocation failed; may be
called from other threads, must be called without any locks held */
void
libsqfs_image_out_of_memory(libsqfs_image_t image, bool fatal);

/* flag other error state on image, with description; may be called from
another thread, must be called without any locks held */
void
libsqfs_image_flag_error(libsqfs_image_t image, const char description[], bool fatal);

bool
libsqfs_write_superblock(libsqfs_image_t image);

void
libsqfs_reserve_superblock(libsqfs_image_t image);

#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t cpu_to_le16(uint16_t v) {return v;}
static inline uint32_t cpu_to_le32(uint32_t v) {return v;}
static inline uint64_t cpu_to_le64(uint64_t v) {return v;}
#elif __BYTE_ORDER == __BIG_ENDIAN
#include <byteswap.h>
static inline uint16_t cpu_to_le16(uint16_t v) {return bswap_16(v);}
static inline uint32_t cpu_to_le32(uint32_t v) {return bswap_32(v);}
static inline uint64_t cpu_to_le64(uint64_t v) {return bswap_64(v);}
#else
#error Unknown endian
#endif

#endif
