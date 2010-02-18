#include "internal.h"
#include "squashfs_fs.h"

void
libsqfs_inode_destroy(libsqfs_inode_t inode)
{
	inode->vmt->destroy(inode);
}

static libsqfs_inodeattr_t
libsqfs_inodeattr_create(libsqfs_image_t image)
{
	libsqfs_inodeattr_t attr = malloc(sizeof(*attr));
	if (!attr) return 0;
	
	attr->prev = image->inodeattrs.last;
	attr->next = 0;
	if (image->inodeattrs.last) image->inodeattrs.last->next = attr;
	else image->inodeattrs.first = attr;
	image->inodeattrs.last = attr;
	
	return attr;
}

libsqfs_inodeattr_t
libsqfs_inodeattr_create_simple(libsqfs_image_t image, uid_t uid, gid_t gid, mode_t mode, time_t ctime)
{
	libsqfs_inodeattr_t attr = libsqfs_inodeattr_create(image);
	if (!attr) return 0;
	
	attr->uid = uid;
	attr->gid = gid;
	attr->mode = mode;
	attr->ctime = ctime;
	
	attr->mapped_uid = libsqfs_idtable_map(&image->idtable, uid);
	attr->mapped_gid = libsqfs_idtable_map(&image->idtable, gid);
	
	if (attr->mapped_uid == -1 || attr->mapped_gid == -1) {
		/* FIXME: set error on image */
		return 0;
	}
	
	return attr;
}

void
libsqfs_inodeattr_destroy(libsqfs_inodeattr_t attr)
{
	free(attr);
}

void
libsqfs_inode_init(libsqfs_image_t image, libsqfs_inode_t inode)
{
	inode->prev = image->inodes.last;
	inode->next = 0;
	inode->image = image;
	
	if (image->inodes.last) image->inodes.last->next = inode;
	else image->inodes.first = inode;
	image->inodes.last = inode;
	
	image->inodes.count++;
	inode->inode_number = image->inodes.count;
	inode->squashfs_inode = -1;
}


void
libsqfs_inode_table_layout(libsqfs_image_t image, libsqfs_inode_table * inode_table)
{
	libsqfs_off_t offset = 0;
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		size_t size = inode->vmt->encoded_size(inode);
		inode->squashfs_inode = (offset & (SQUASHFS_METADATA_SIZE-1))
			| ((offset/SQUASHFS_METADATA_SIZE)<<16);
		
		offset += size;
		inode = inode->next;
	}
	inode_table->size = offset;
}

bool
libsqfs_inode_table_write(libsqfs_image_t image, libsqfs_inode_table * inode_table)
{
	void * data, * current;
	data = malloc(inode_table->size);
	if (!data) return false;
	
	current = data;
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		inode->vmt->encode(inode, current);
		current = inode->vmt->encoded_size(inode) + (char *) current;
		inode = inode->next;
	}
	
	libsqfs_off_t offset = libsqfs_write_metatable(image, data, inode_table->size, image->options.inode_compression, false);
	
	inode_table->offset = offset;
	free(data);
	
	return offset != -1;
}
