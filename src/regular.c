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
#include <limits.h>

#include "squashfs_fs.h"

struct libsqfs_regular_inode {
	struct libsqfs_inode base;
	
	libsqfs_off_t file_size, sparse_size;
	size_t nblocks;
	libsqfs_full_block ** blocks;
	libsqfs_fragment_piece * tail_piece;
};

static void
libsqfs_regular_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_regular_inode_t reg = (libsqfs_regular_inode_t) inode;
	free(reg->blocks);
	free(reg);
}

static bool
libsqfs_regular_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_regular_inode_t reg = (libsqfs_regular_inode_t) inode;
	libsqfs_image_t image = reg->base.image;
	
	reg->base.encoded_type = SQUASHFS_FILE_TYPE;
	reg->base.encoded_base_type = SQUASHFS_FILE_TYPE;
	if (reg->sparse_size || reg->base.nlink != 1 || reg->file_size > UINT_MAX)
		reg->base.encoded_type = SQUASHFS_LREG_TYPE;
	if (reg->base.attr->xattrset)
		reg->base.encoded_type = SQUASHFS_LREG_TYPE;
	if (reg->nblocks && reg->blocks[0]->dst.offset > UINT_MAX)
		reg->base.encoded_type = SQUASHFS_LREG_TYPE;
	
	if (reg->base.encoded_type == SQUASHFS_FILE_TYPE) {
		struct squashfs_reg_inode_header hdr;
		hdr.inode_type = cpu_to_le16(reg->base.encoded_type);
		hdr.mode = cpu_to_le16(reg->base.attr->mode);
		hdr.uid = cpu_to_le16(reg->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(reg->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(reg->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(reg->base.inode_number);
		
		if (reg->nblocks)
			hdr.start_block = cpu_to_le32(reg->blocks[0]->dst.offset);
		else
			hdr.start_block = cpu_to_le32(0);
		if (reg->tail_piece) {
			hdr.fragment = cpu_to_le32(reg->tail_piece->fragment_block->index);
			hdr.offset = cpu_to_le32(reg->tail_piece->offset);
		} else {
			hdr.fragment = cpu_to_le32(-1);
			hdr.offset = cpu_to_le32(0);
		}
		hdr.file_size = cpu_to_le32(reg->file_size);
		if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &reg->base.inode_table_entry))
			return false;
	} else {
		struct squashfs_lreg_inode_header hdr;
		hdr.inode_type = cpu_to_le16(reg->base.encoded_type);
		hdr.mode = cpu_to_le16(reg->base.attr->mode);
		hdr.uid = cpu_to_le16(reg->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(reg->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(reg->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(reg->base.inode_number);
		
		hdr.file_size = cpu_to_le64(reg->file_size);
		hdr.sparse = cpu_to_le64(reg->sparse_size);
		hdr.nlink = cpu_to_le32(reg->base.nlink);
		if (reg->nblocks)
			hdr.start_block = cpu_to_le64(reg->blocks[0]->dst.offset);
		else
			hdr.start_block = cpu_to_le64(0);
		if (reg->tail_piece) {
			hdr.fragment = cpu_to_le32(reg->tail_piece->fragment_block->index);
			hdr.offset = cpu_to_le32(reg->tail_piece->offset);
		} else {
			hdr.fragment = cpu_to_le32(-1);
			hdr.offset = cpu_to_le32(0);
		}
		if (reg->base.attr->xattrset)
			hdr.xattr = cpu_to_le32(reg->base.attr->xattrset->id);
		else
			hdr.xattr = cpu_to_le32(-1);
		if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &reg->base.inode_table_entry))
			return false;
	}
	
	/* A file stored entirely in a fragment has no block-size entries. */
	if (!reg->nblocks)
		return true;

	unsigned int block_info[reg->nblocks];
	size_t n;
	for(n=0; n<reg->nblocks; n++) {
		uint32_t value = reg->blocks[n]->dst.size;
		if (!reg->blocks[n]->dst.compressed)
			value |= SQUASHFS_COMPRESSED_BIT_BLOCK;
		block_info[n] = cpu_to_le32(value);
	}
	
	return libsqfs_metatable_append(&image->inode_table, block_info, sizeof(block_info), 0);
}

const libsqfs_inode_vmt libsqfs_regular_inode_vmt = {
	.serialize = &libsqfs_regular_inode_serialize,
	.destroy = &libsqfs_regular_inode_destroy
};

libsqfs_regular_inode_t
libsqfs_regular_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, libsqfs_data_t data)
{
	libsqfs_off_t file_size = libsqfs_data_get_size(data);
	if (file_size == -1) return 0;
	
	size_t nblocks, tail_size;
	
	switch(image->options->fragments) {
		case libsqfs_fragments_never:
			nblocks = (file_size + image->options->block_size-1) / image->options->block_size;
			tail_size = 0;
			break;
		default:
		case libsqfs_fragments_small:
			if (file_size < image->options->block_size) {
				nblocks = 0;
				tail_size = file_size;
			} else {
				nblocks = (file_size + image->options->block_size-1) / image->options->block_size;
				tail_size = 0;
			}
			break;
		case libsqfs_fragments_always:
			nblocks = file_size / image->options->block_size;
			tail_size = file_size % image->options->block_size;
			break;
	}
	
	libsqfs_regular_inode_t reg = malloc(sizeof(*reg));
	if (!reg) return 0;
	
	reg->nblocks = nblocks;
	reg->blocks = malloc(nblocks * sizeof(reg->blocks[0]));
	if (!reg->blocks) {
		libsqfs_image_out_of_memory(image, false);
		free(reg);
		return 0;
	}
	
	memset(reg->blocks, 0, nblocks * sizeof(reg->blocks[0]));
	
	reg->base.vmt = &libsqfs_regular_inode_vmt;
	libsqfs_inode_init(image, &reg->base, attr);
	reg->file_size = file_size;
	reg->sparse_size = 0;
	
	size_t n;
	for(n=0; n<nblocks; n++) {
		libsqfs_off_t offset = n * (libsqfs_off_t)image->options->block_size;
		size_t size = image->options->block_size;
		if (file_size-offset < image->options->block_size) size = file_size - offset;
		libsqfs_data_piece p;
		p.data = data;
		p.size = size;
		p.offset = offset;
		libsqfs_full_block * block = libsqfs_bulkdata_sumbit(&image->bulkdata, p);
		if (!block) {
			libsqfs_image_out_of_memory(image, false);
			return 0;
		}
		reg->blocks[n] = block;
	}
	
	if (tail_size) {
		libsqfs_data_piece p;
		p.data = data;
		p.size = tail_size;
		p.offset = reg->file_size - tail_size;
		reg->tail_piece = libsqfs_bulkdata_submit_fragment(&image->bulkdata, p);
		
		if (!reg->tail_piece) {
			libsqfs_image_out_of_memory(image, false);
			return 0;
		}
	} else reg->tail_piece = 0;
	
	return reg;
}

libsqfs_inode_t
libsqfs_regular_inode_downcast(libsqfs_regular_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}
