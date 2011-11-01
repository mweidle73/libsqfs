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

static void
libsqfs_image_options_defaults(libsqfs_image_options_t options)
{
	options->inode_compression = true;
	options->data_compression = true;
	options->fragment_compression = true;
	options->fragments = libsqfs_fragments_small;
	options->exportable = false;
	options->compressor = &libsqfs_compressor_zlib;
	options->padding = true;
	options->block_size_log = 17 /* SQUASHFS_FILE_LOG */;
	options->block_size = 1 << options->block_size_log;
}

libsqfs_image_options_t
libsqfs_image_options_create(void)
{
	libsqfs_image_options_t options = malloc(sizeof(*options));
	if (!options) return 0;
	libsqfs_image_options_defaults(options);
	return options;
}

void
libsqfs_image_options_destroy(libsqfs_image_options_t options)
{
	free(options);
}

void
libsqfs_image_options_set_inode_compression(libsqfs_image_options_t options, bool compress)
{
	options->inode_compression = compress;
}

void
libsqfs_image_options_set_data_compression(libsqfs_image_options_t options, bool compress)
{
	options->data_compression = compress;
}

void
libsqfs_image_options_set_fragment_compression(libsqfs_image_options_t options, bool compress)
{
	options->fragment_compression = compress;
}

void
libsqfs_image_options_set_exportable(libsqfs_image_options_t options, bool exportable)
{
	options->exportable = exportable;
}

void
libsqfs_image_options_set_padding(libsqfs_image_options_t options, bool padding)
{
	options->padding = padding;
}

void
libsqfs_image_options_set_fragment_option(libsqfs_image_options_t options, libsqfs_fragments_option fragments)
{
	options->fragments = fragments;
}

void
libsqfs_image_options_set_block_size(libsqfs_image_options_t options, size_t block_size)
{
	if (block_size & (block_size-1)) return;
	if ((block_size < 4096) || (block_size > 1048576)) return;
	
	options->block_size = block_size;
	options->block_size_log = 0;
	while( (block_size >>= 1) )
		options->block_size_log ++;
}

void
libsqfs_image_options_set_compressor(libsqfs_image_options_t options, const struct _libsqfs_compressor * compressor)
{
	options->compressor = compressor;
}


libsqfs_image_t
libsqfs_image_create(libsqfs_destination_t destination, libsqfs_image_options_t options)
{
	libsqfs_image_t image = malloc(sizeof(*image));
	if (!image) {
		errno = ENOMEM;
		return 0;
	}
	
	if (options) image->options = *options;
	else libsqfs_image_options_defaults(&image->options);
	
	image->compressor = libsqfs_compressor_open(image->options.compressor);
	if (!image->compressor) {
		free(image);
		errno = ENOMEM;
		return 0;
	}
	
	image->dst = destination;
	image->size = 0;
	image->creation_time = time(NULL);
	image->state = libsqfs_image_building;
	image->error_msg = 0;
	pthread_mutex_init(&image->state_mutex, 0);
	image->dataitems.first = image->dataitems.last = 0;
	image->inodeattrs.first = image->inodeattrs.last = 0;
	image->inodes.first = image->inodes.last = 0;
	image->inodes.count = 0;
	
	image->root = 0;
	
	libsqfs_bulkdata_init(&image->bulkdata, image->options.data_compression, image->options.fragment_compression);
	libsqfs_idtable_init(&image->idtable);
	libsqfs_metatable_init(&image->dir_table, image->options.inode_compression ? image->compressor : 0);
	libsqfs_metatable_init(&image->inode_table, image->options.inode_compression ? image->compressor : 0);
	libsqfs_export_table_init(&image->export_table);
	libsqfs_xattr_table_init(&image->xattr_table, image->options.data_compression ? image->compressor : 0);
	
	libsqfs_threadpool_init(&image->threadpool);
	
	libsqfs_reserve_superblock(image);
	
	return image;
}

libsqfs_image_state_t
libsqfs_image_state(libsqfs_image_t image)
{
	return image->state;
}

libsqfs_image_state_t
libsqfs_image_close(libsqfs_image_t image)
{
	libsqfs_image_state_t state = libsqfs_image_finalize(image);
	
	libsqfs_image_error_clear(image);
	
	libsqfs_threadpool_fini(&image->threadpool);
	
	libsqfs_data_t data = image->dataitems.first;
	while(data) {
		libsqfs_data_t next = data->next;
		libsqfs_data_destroy(data);
		data = next;
	}
	
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		libsqfs_inode_t next = inode->next;
		libsqfs_inode_destroy(inode);
		inode = next;
	}
	
	libsqfs_inodeattr_t inodeattr = image->inodeattrs.first;
	while(inodeattr) {
		libsqfs_inodeattr_t next = inodeattr->next;
		libsqfs_inodeattr_destroy(inodeattr);
		inodeattr = next;
	}
	
	libsqfs_xattr_table_fini(&image->xattr_table);
	libsqfs_bulkdata_fini(&image->bulkdata);
	libsqfs_metatable_fini(&image->dir_table);
	libsqfs_metatable_fini(&image->inode_table);
	libsqfs_idtable_destroy(&image->idtable);
	libsqfs_compressor_instance_destroy(image->compressor);
	
	free(image);
	
	return state;
}

libsqfs_image_state_t
libsqfs_image_finalize(libsqfs_image_t image)
{
	if (image->state != libsqfs_image_building)
		return image->state;
	
	bool success = true;
	
	if (!image->root) {
		libsqfs_image_flag_error(image, "No root directory set", true);
		success = false;
	}
	
	success = success && libsqfs_bulkdata_seal(&image->bulkdata);
	success = success && libsqfs_bulkdata_finish(&image->bulkdata, image);
	
	if (success) libsqfs_inode_serialize(libsqfs_directory_inode_downcast(image->root));
	success = success && libsqfs_metatable_write(&image->inode_table, image);
	success = success && libsqfs_metatable_write(&image->dir_table, image);
	
	success = success && libsqfs_bulkdata_write_fragment_table(&image->bulkdata, image);
	if (image->options.exportable)
		success = success && libsqfs_export_table_write(image, &image->export_table);
	success = success && libsqfs_idtable_write(image, &image->idtable);
	success = success && libsqfs_xattr_table_write(&image->xattr_table, image);
	success = success && libsqfs_write_superblock(image);
	
	if (success && image->options.padding) {
		libsqfs_off_t padded_size = (image->size + 4095) & ~4095;
		if (padded_size != image->size)
			if (libsqfs_truncate(image->dst, padded_size))
				return false;
	}
	
	/* note: since previous error messages are not overwritten,
	this will only change the error message if something went wrong
	before without specifying on informational message */
	if (success) image->state = libsqfs_image_finalized;
	else libsqfs_image_flag_error(image, "Cannot finalize image (unknown error)", true);
	
	return image->state;
}

void
libsqfs_image_abort(libsqfs_image_t image)
{
	libsqfs_image_flag_error(image, "User abort", true);
}

bool
libsqfs_image_set_root(libsqfs_image_t image, libsqfs_directory_inode_t root)
{
	if (image->root || image->state != libsqfs_image_building) return false;
	image->root = root;
	root->base.nlink++;
	return true;
}

libsqfs_directory_inode_t
libsqfs_image_get_root(libsqfs_image_t image)
{
	return image->root;
}

libsqfs_off_t
libsqfs_image_reserve(libsqfs_image_t image, size_t bytes)
{
	libsqfs_off_t current = image->size;
	image->size += bytes;
	return current;
}

static void * 
libsqfs_image_thread_function(void * arg)
{
	libsqfs_image_t image = arg;
	libsqfs_compressor_instance * ci  = libsqfs_compressor_open(image->options.compressor);
	if (!ci) {
		libsqfs_image_out_of_memory(image, true);
		return 0;
	}
	
	libsqfs_bulkdata_process(&image->bulkdata, ci, image);
	
	libsqfs_compressor_instance_destroy(ci);
	
	return 0;
}

ssize_t
libsqfs_image_spawn_threads(libsqfs_image_t image, size_t count)
{
	size_t spawned = 0;
	while(spawned < count) {
		if (!libsqfs_threadpool_spawn_worker(&image->threadpool, &libsqfs_image_thread_function, image))
			break;
		spawned ++;
	}
	
	return spawned;
}

ssize_t
libsqfs_image_auto_spawn_threads(libsqfs_image_t image)
{
	return libsqfs_threadpool_auto_spawn_worker(&image->threadpool, libsqfs_image_thread_function, image);
}

static const char out_of_memory_msg[] = "Memory allocation failed";

static void
libsqfs_image_cancel_pending_operations(libsqfs_image_t image)
{
	libsqfs_bulkdata_abort(&image->bulkdata);
}

void
libsqfs_image_out_of_memory(libsqfs_image_t image, bool fatal)
{
	pthread_mutex_lock(&image->state_mutex);
	if (!image->error_msg)
		image->error_msg = out_of_memory_msg;
	if (fatal) image->state = libsqfs_image_fatal_error;
	pthread_mutex_unlock(&image->state_mutex);
	
	if (fatal) libsqfs_image_cancel_pending_operations(image);
}

void
libsqfs_image_flag_error(libsqfs_image_t image, const char description[], bool fatal)
{
	pthread_mutex_lock(&image->state_mutex);
	if (!image->error_msg) {
		image->error_msg = strdup(description);
		if (!image->error_msg) image->error_msg = out_of_memory_msg;
	}
	if (fatal) image->state = libsqfs_image_fatal_error;
	pthread_mutex_unlock(&image->state_mutex);
	
	if (fatal) libsqfs_image_cancel_pending_operations(image);
}

void
libsqfs_image_error_clear(libsqfs_image_t image)
{
	pthread_mutex_lock(&image->state_mutex);
	if (image->error_msg && image->error_msg != out_of_memory_msg)
		free((char *)image->error_msg);
	image->error_msg = 0;
	pthread_mutex_unlock(&image->state_mutex);
}

const char *
libsqfs_image_error_message(libsqfs_image_t image)
{
	return image->error_msg;
}
