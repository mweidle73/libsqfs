#include "internal.h"

#include <string.h>

#include "squashfs_fs.h"

static size_t
libsqfs_directory_encoded_size(libsqfs_directory_inode_t dir)
{
	size_t size = 0;
	libsqfs_directory_entry * entry = dir->entries.first;
	while(entry) {
		size_t entry_size = sizeof(struct squashfs_dir_entry) + strlen(entry->name);
		size += entry_size;
	}
	if (size) size += sizeof(struct squashfs_dir_header);
	
	return size;
}

static void
libsqfs_directory_encode(libsqfs_directory_inode_t dir, void * dst)
{
}

static void
libsqfs_directory_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	/* FIXME: free entries */
	free(dir);
}

static size_t
libsqfs_directory_inode_encoded_size(libsqfs_inode_t inode)
{
	return sizeof(struct squashfs_dir_inode_header);
}

static void
libsqfs_directory_inode_encode(libsqfs_inode_t inode, void * dst)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	struct squashfs_dir_inode_header * hdr = dst;
	
	hdr->inode_type = cpu_to_le16(SQUASHFS_DIR_TYPE);
	hdr->mode = cpu_to_le16(dir->attr->mode);
	hdr->uid = cpu_to_le16(dir->attr->mapped_uid);
	hdr->guid = cpu_to_le16(dir->attr->mapped_gid);
	hdr->mtime = cpu_to_le32(dir->attr->ctime);
	hdr->inode_number = cpu_to_le32(dir->inode_number);
	
	hdr->start_block = 0; /* FIXME: points to block in dir table */
	hdr->offset = cpu_to_le16(0); /* points to block in dir table */
	
	hdr->nlink = cpu_to_le32(dir->nlink);
	
	hdr->file_size = cpu_to_le16(libsqfs_directory_encoded_size(dir) + 3);
	
	if (dir->parent)
		hdr->parent_inode = cpu_to_le32(dir->parent->inode_number);
	else
		hdr->parent_inode = cpu_to_le32(dir->image->inodes.count+1);
		/* this is the "invalid inode marker" */
}

const libsqfs_inode_vmt libsqfs_directory_inode_vmt = {
	.destroy = &libsqfs_directory_inode_destroy,
	.encoded_size = &libsqfs_directory_inode_encoded_size,
	.encode = &libsqfs_directory_inode_encode
};

libsqfs_directory_inode_t
libsqfs_directory_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr)
{
	libsqfs_directory_inode_t dir = malloc(sizeof(*dir));
	if (!dir) return 0;
	
	dir->vmt = &libsqfs_directory_inode_vmt;
	dir->attr = attr;
	dir->nlink = 1;
	dir->entries.first = dir->entries.last = 0;
	dir->inode_table_offset = -1;
	libsqfs_inode_init(image, (libsqfs_inode_t) dir);
	return dir;
}

void
libsqfs_directory_table_layout(libsqfs_image_t image, libsqfs_directory_table * dir_table)
{
	dir_table->size = 0;
	libsqfs_inode_t inode;
	
	for(inode = image->inodes.first; inode; inode=inode->next) {
		if (inode->vmt != &libsqfs_directory_inode_vmt) continue;
		libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
		
		dir->dir_table_offset = dir_table->size;
		dir_table->size += libsqfs_directory_encoded_size(dir);
	}
}

bool
libsqfs_directory_table_write(libsqfs_image_t image, libsqfs_directory_table * dir_table)
{
	if (!dir_table->size) {
		dir_table->offset = libsqfs_image_reserve(image, 0);
		return true;
	}
	return true;
}

