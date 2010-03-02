#include "internal.h"

#include <string.h>

#include "squashfs_fs.h"

struct _libsqfs_regular_inode {
	LIBSQFS_INODE_COMMON
	
	libsqfs_off_t file_size;
	size_t nblocks;
	libsqfs_full_block ** blocks;
	libsqfs_fragment_piece * tail_piece;
};

static size_t
libsqfs_regular_inode_encoded_size(libsqfs_regular_inode_t reg)
{
	/* FIXME: sometimes need LREG_TYPE */
	return sizeof(struct squashfs_reg_inode_header) + reg->nblocks * 4;
}

static void
libsqfs_regular_inode_encode(libsqfs_regular_inode_t reg, void * dst)
{
	
	struct squashfs_reg_inode_header * hdr = dst;
	dst = hdr + 1;
	
	/* FIXME: sometimes need LREG_TYPE */
	reg->encoded_type = SQUASHFS_FILE_TYPE;
	
	hdr->inode_type = cpu_to_le16(reg->encoded_type);
	hdr->mode = cpu_to_le16(reg->attr->mode);
	hdr->uid = cpu_to_le16(reg->attr->mapped_uid);
	hdr->guid = cpu_to_le16(reg->attr->mapped_gid);
	hdr->mtime = cpu_to_le32(reg->attr->ctime);
	hdr->inode_number = cpu_to_le32(reg->inode_number);
	
	if (reg->nblocks)
		hdr->start_block = cpu_to_le32(reg->blocks[0]->dst.offset);
	else
		hdr->start_block = cpu_to_le32(0);
	if (reg->tail_piece) {
		hdr->fragment = cpu_to_le32(reg->tail_piece->fragment_block->index);
		hdr->offset = cpu_to_le32(reg->tail_piece->offset);
	} else {
		hdr->fragment = cpu_to_le32(-1);
		hdr->offset = cpu_to_le32(0);
	}
	hdr->file_size = cpu_to_le32(reg->file_size);
	
	unsigned int * block_info = dst;
	size_t n;
	for(n=0; n<reg->nblocks; n++) {
		uint32_t value = reg->blocks[n]->dst.size;
		if (!reg->blocks[n]->dst.compressed)
			value |= SQUASHFS_COMPRESSED_BIT_BLOCK;
		*block_info = cpu_to_le32(value);
		block_info++;
	}
}

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
	libsqfs_image_t image = reg->image;
	
	size_t encoded_size = libsqfs_regular_inode_encoded_size(reg);
	char buffer[encoded_size];
	libsqfs_regular_inode_encode(reg, buffer);
	
	if (!libsqfs_metatable_append(&image->inode_table.tab, buffer, encoded_size, &reg->inode_table_entry)) return false;
	return true;
}

static const libsqfs_inode_vmt libsqfs_regular_inode_vmt = {
	.serialize = &libsqfs_regular_inode_serialize,
	.destroy = &libsqfs_regular_inode_destroy
};

libsqfs_regular_inode_t
libsqfs_regular_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, libsqfs_data_t data)
{
	libsqfs_off_t file_size = libsqfs_data_get_size(data);
	/* FIXME: flag error on image */
	if (file_size==-1) return 0;
	
	size_t nblocks, tail_size;
	
	switch(image->options.fragments) {
		case libsqfs_fragments_never:
			nblocks = (file_size + image->options.block_size-1) / image->options.block_size;
			tail_size = 0;
			break;
		default:
		case libsqfs_fragments_small:
			if (file_size < image->options.block_size) {
				nblocks = 0;
				tail_size = file_size;
			} else {
				nblocks = (file_size + image->options.block_size-1) / image->options.block_size;
				tail_size = 0;
			}
			break;
		case libsqfs_fragments_always:
			nblocks = file_size / image->options.block_size;
			tail_size = file_size % image->options.block_size;
			break;
	}
	
	libsqfs_regular_inode_t reg = malloc(sizeof(*reg));
	if (!reg) return 0;
	
	reg->nblocks = nblocks;
	reg->blocks = malloc(nblocks * sizeof(reg->blocks[0]));
	if (!reg->blocks) {
		/* FIXME: flag error on image */
		free(reg);
		return 0;
	}
	
	memset(reg->blocks, 0, nblocks * sizeof(reg->blocks[0]));
	
	reg->vmt = &libsqfs_regular_inode_vmt;
	reg->attr = attr;
	reg->nlink = 0;
	reg->file_size = file_size;
	libsqfs_inode_init(image, (libsqfs_inode_t) reg);
	
	size_t n;
	for(n=0; n<nblocks; n++) {
		libsqfs_off_t offset = n * (libsqfs_off_t)image->options.block_size;
		size_t size = image->options.block_size;
		if (file_size-offset < image->options.block_size) size = file_size - offset;
		libsqfs_data_piece p;
		p.data = data;
		p.size = size;
		p.offset = offset;
		libsqfs_full_block * block = libsqfs_bulkdata_sumbit(&image->bulkdata, p);
		if (!block) {
			libsqfs_image_out_of_memory(image);
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
			libsqfs_image_out_of_memory(image);
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
