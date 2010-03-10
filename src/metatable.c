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

#include "metatable.h"

#include <string.h>

#include "internal.h"

static void
libsqfs_metablock_destroy(libsqfs_metablock * block)
{
	free(block);
}

void
libsqfs_metatable_init(libsqfs_metatable * tab, libsqfs_compressor_instance * compressor)
{
	tab->first = tab->last = tab->opened_block = 0;
	tab->nmetablocks = 0;
	tab->opened_block_fill = 0;
	tab->compressor = compressor;
	tab->size = 0;
	tab->offset = (libsqfs_off_t) -1;
}

void
libsqfs_metatable_fini(libsqfs_metatable * tab)
{
	if (tab->opened_block)
		libsqfs_metablock_destroy(tab->opened_block);
	libsqfs_metablock * block = tab->first;
	while(block) {
		libsqfs_metablock * next = block->next;
		libsqfs_metablock_destroy(block);
		block = next;
	}
}

static void
libsqfs_metatable_flush(libsqfs_metatable * tab)
{
	if (!tab->opened_block) return;
	
	libsqfs_metablock * block = tab->opened_block;
	block->size = tab->opened_block_fill;
	tab->opened_block = 0;
	tab->opened_block_fill = 0;
	
	char buffer[SQUASHFS_METADATA_SIZE];
	ssize_t compressed_size = -1;
	
	if (tab->compressor)
		compressed_size = libsqfs_compressor_instance_compress(tab->compressor,
			buffer, block->size, block->data, block->size);
	
	if (compressed_size != -1) {
		memcpy(block->data, buffer, compressed_size);
		block->size = compressed_size;
		block->compressed = true;
	}
	
	tab->size += block->size + 2;
	block->prev = tab->last;
	block->next = 0;
	if (tab->last) tab->last->next = block;
	else tab->first = block;
	tab->last = block;
	tab->nmetablocks ++;
}

static libsqfs_metablock *
libsqfs_metatable_getblock(libsqfs_metatable * tab)
{
	if (tab->opened_block) return tab->opened_block;
	
	libsqfs_metablock * block = malloc(sizeof(*block));
	if (!block) return false;
	
	block->prev = block->next = 0;
	block->size = SQUASHFS_METADATA_SIZE;
	block->offset = tab->size;
	memset(block->data, 0, SQUASHFS_METADATA_SIZE);
	block->compressed = false;
	
	tab->opened_block = block;
	tab->opened_block_fill = 0;
	return block;
}

bool
libsqfs_metatable_append(libsqfs_metatable * tab, const void * data, size_t count, 
libsqfs_metatable_entry * pos)
{
	if (pos) {
		pos->block = tab->size;
		pos->offset = tab->opened_block_fill;
	}
	
	while(count) {
		libsqfs_metablock * block = libsqfs_metatable_getblock(tab);
		
		if (!block) return false;
		size_t to_copy = SQUASHFS_METADATA_SIZE - tab->opened_block_fill;
		if (to_copy > count) to_copy = count;
		
		memcpy(block->data + tab->opened_block_fill, data, to_copy);
		
		tab->opened_block_fill += to_copy;
		count -= to_copy;
		data = to_copy + (char *)data;
		
		if (tab->opened_block_fill == SQUASHFS_METADATA_SIZE)
			libsqfs_metatable_flush(tab);
	}
	
	return true;
}

bool
libsqfs_metatable_write(libsqfs_metatable * tab, libsqfs_image_t image)
{
	libsqfs_metatable_flush(tab);
	
	libsqfs_off_t start = libsqfs_image_reserve(image, tab->size);
	libsqfs_off_t offset = start;
	
	libsqfs_metablock * block = tab->first;
	while(block) {
		/* COMPRESSED_BIT actually means "uncompressed"... */
		uint16_t header = block->size |
			(block->compressed ? 0 : SQUASHFS_COMPRESSED_BIT);
		header = cpu_to_le16(header);
		
		ssize_t written;
		written = libsqfs_pwrite(image->dst, &header, sizeof(header), offset);
		/* FIXME: flag error reason on image */
		if (written != 2) return false;
		written = libsqfs_pwrite(image->dst, block->data, block->size, offset+2);
		/* FIXME: flag error reason on image */
		if (written != block->size) return false;
		
		offset = offset + block->size + 2;
		block = block->next;
	}
	
	tab->offset = start;
	
	return true;
}

bool
libsqfs_metatable_write_with_index(libsqfs_metatable * tab, libsqfs_image_t image)
{
	if (!libsqfs_metatable_write(tab, image)) return false;
	
	libsqfs_off_t table_base = tab->offset;
	uint64_t index_table[tab->nmetablocks];
	libsqfs_off_t index_table_base = libsqfs_image_reserve(image, sizeof(index_table));
	size_t n;
	libsqfs_metablock * block = tab->first;
	for(n=0; n<tab->nmetablocks; n++) {
		index_table[n] = cpu_to_le64(table_base + block->offset);
		block = block->next;
	}
	
	ssize_t written = libsqfs_pwrite(image->dst, index_table, sizeof(index_table), index_table_base);
	/* FIXME: flag error reason on image */
	if (written != sizeof(index_table)) return false;
	
	tab->offset = index_table_base;
	return true;
}
