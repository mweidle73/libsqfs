#include "internal.h"

libsqfs_image_t
libsqfs_image_create(libsqfs_destination_t destination)
{
	libsqfs_image_t image = malloc(sizeof(*image));
	if (!image) {
		errno = ENOMEM;
		return 0;
	}
	
	image->dst = destination;
	image->size = 0;
	image->creation_time = 0;
	image->block_size_log = 17 /* SQUASHFS_FILE_LOG */;
	image->block_size = 1 << image->block_size_log;
	image->compression_method = 1 /* ZLIB_COMPRESSION */;
	image->state = libsqfs_image_building;
	image->inodeattrs.first = image->inodeattrs.last = 0;
	image->inodes.first = image->inodes.last = 0;
	image->inodes.count = 0;
	image->root = 0;
	
	libsqfs_idtable_init(&image->idtable);
	
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
	
	free(image);
	
	return state;
}

libsqfs_image_state_t
libsqfs_image_finalize(libsqfs_image_t image)
{
	if (image->state != libsqfs_image_building)
		return image->state;
	
	bool success = true;
	
	libsqfs_inode_table_layout(image, &image->inode_table);
	libsqfs_directory_table_layout(image, &image->dir_table);
	
	success = success && libsqfs_inode_table_write(image, &image->inode_table);
	success = success && libsqfs_directory_table_write(image, &image->dir_table);
	success = success && libsqfs_fragment_table_write(image, &image->frag_table);
	success = success && libsqfs_idtable_write(image, &image->idtable);
	success = success && libsqfs_write_superblock(image);
	
	if (success) image->state = libsqfs_image_finalized;
	else image->state = libsqfs_image_error;
	
	return image->state;
}

bool
libsqfs_image_set_root(libsqfs_image_t image, libsqfs_directory_inode_t root)
{
	if (image->root || image->state != libsqfs_image_building) return false;
	image->root = root;
	root->nlink++;
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
