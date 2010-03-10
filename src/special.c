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

#include "squashfs_fs.h"


struct _libsqfs_symlink_inode {
	LIBSQFS_INODE_COMMON
	
	char * name;
};

static bool
libsqfs_symlink_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_symlink_inode_t lnk = (libsqfs_symlink_inode_t) inode;
	libsqfs_image_t image = lnk->image;
	
	lnk->encoded_type = SQUASHFS_SYMLINK_TYPE;
	
	size_t namelen = strlen(lnk->name);
	
	struct squashfs_symlink_inode_header hdr;
	
	hdr.inode_type = cpu_to_le16(lnk->encoded_type);
	hdr.mode = cpu_to_le16(lnk->attr->mode);
	hdr.uid = cpu_to_le16(lnk->attr->mapped_uid);
	hdr.guid = cpu_to_le16(lnk->attr->mapped_gid);
	hdr.mtime = cpu_to_le32(lnk->attr->ctime);
	hdr.inode_number = cpu_to_le32(lnk->inode_number);
	hdr.nlink = cpu_to_le32(lnk->nlink);
	
	hdr.symlink_size = cpu_to_le32(namelen);
	
	if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &lnk->inode_table_entry)) return false;
	if (!libsqfs_metatable_append(&image->inode_table, lnk->name, namelen, 0)) return false;
	
	return true;
}

static void
libsqfs_symlink_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_symlink_inode_t lnk = (libsqfs_symlink_inode_t) inode;
	free(lnk->name);
	free(lnk);
}

static const libsqfs_inode_vmt libsqfs_symlink_inode_vmt = {
	.serialize = &libsqfs_symlink_inode_serialize,
	.destroy = &libsqfs_symlink_inode_destroy
};

libsqfs_symlink_inode_t
libsqfs_symlink_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, const char * name)
{
	libsqfs_symlink_inode_t lnk = malloc(sizeof(*lnk));
	if (!lnk) {
		libsqfs_image_out_of_memory(image, false);
		return 0;
	}
	
	lnk->name = strdup(name);
	if (!lnk->name) {
		free(lnk);
		libsqfs_image_out_of_memory(image, false);
		return 0;
	}
	
	lnk->vmt = &libsqfs_symlink_inode_vmt;
	lnk->attr = attr;
	lnk->nlink = 0;
	libsqfs_inode_init(image, (libsqfs_inode_t) lnk);
	
	return lnk;
}

libsqfs_inode_t
libsqfs_symlink_inode_downcast(libsqfs_symlink_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}

struct _libsqfs_device_inode {
	LIBSQFS_INODE_COMMON
	
	char type;
	unsigned int major, minor;
};

static bool
libsqfs_device_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_device_inode_t dev = (libsqfs_device_inode_t) inode;
	libsqfs_image_t image = dev->image;
	
	struct squashfs_dev_inode_header hdr;
	
	if (dev->type == 'c')
		dev->encoded_type = SQUASHFS_CHRDEV_TYPE;
	else
		dev->encoded_type = SQUASHFS_BLKDEV_TYPE;
	
	hdr.inode_type = cpu_to_le16(dev->encoded_type);
	hdr.mode = cpu_to_le16(dev->attr->mode);
	hdr.uid = cpu_to_le16(dev->attr->mapped_uid);
	hdr.guid = cpu_to_le16(dev->attr->mapped_gid);
	hdr.mtime = cpu_to_le32(dev->attr->ctime);
	hdr.inode_number = cpu_to_le32(dev->inode_number);
	hdr.nlink = cpu_to_le32(dev->nlink);
	
	hdr.rdev = cpu_to_le32(
		(dev->major << 8) | (dev->minor & 0xff) | ((dev->minor & ~0xff) << 12)
	);
	
	if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &dev->inode_table_entry)) return false;
	
	return true;
}

static void
libsqfs_device_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_device_inode_t dev = (libsqfs_device_inode_t) inode;
	free(dev);
}

static const libsqfs_inode_vmt libsqfs_device_inode_vmt = {
	.serialize = &libsqfs_device_inode_serialize,
	.destroy = &libsqfs_device_inode_destroy
};

libsqfs_device_inode_t
libsqfs_device_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, char type, unsigned int major, unsigned int minor)
{
	if ((type != 'c') && (type != 'b')) return 0;
	libsqfs_device_inode_t dev = malloc(sizeof(*dev));
	if (!dev) {
		libsqfs_image_out_of_memory(image, false);
		return 0;
	}
	
	dev->vmt = &libsqfs_device_inode_vmt;
	dev->attr = attr;
	dev->nlink = 0;
	dev->major = major;
	dev->minor = minor;
	dev->type = type;
	libsqfs_inode_init(image, (libsqfs_inode_t) dev);
	
	return dev;
}

libsqfs_inode_t
libsqfs_device_inode_downcast(libsqfs_device_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}
