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
		libsqfs_image_flag_error(image, "Too many user/group ids", false);
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
	
	image->inodes.count ++;
	inode->inode_number = image->inodes.count;
	inode->inode_table_entry.block = -1;
	inode->inode_table_entry.offset = -1;
	inode->encoded_type = 0;
}

bool
libsqfs_inode_serialize(libsqfs_inode_t inode)
{
	if (inode->encoded_type != 0) return true;
	return inode->vmt->serialize(inode);
}

void
libsqfs_inode_table_init(libsqfs_inode_table * tab, libsqfs_compressor_instance * compressor)
{
	libsqfs_metatable_init(&tab->tab, compressor);
	tab->offset = -1;
}

void
libsqfs_inode_table_destroy(libsqfs_inode_table * tab)
{
	libsqfs_metatable_destroy(&tab->tab);
}

bool
libsqfs_inode_table_write(libsqfs_image_t image, libsqfs_inode_table * inode_table)
{
	libsqfs_off_t offset;
	offset = libsqfs_metatable_write(&inode_table->tab, image);
	if (offset == -1) return false;
	inode_table->offset = offset;
	return true;
}
