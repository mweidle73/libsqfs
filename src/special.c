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
	struct _libsqfs_inode base;
	
	char * name;
};

static bool
libsqfs_symlink_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_symlink_inode_t lnk = (libsqfs_symlink_inode_t) inode;
	libsqfs_image_t image = lnk->base.image;
	
	/* FIXME: format does not support xattrs on symlinks, but
	security xattrs on symlinks may actually be required :/
	
	if (lnk->base.attr->xattrset)
		lnk->base.encoded_type = SQUASHFS_LSYMLINK_TYPE;
	else
		lnk->base.encoded_type = SQUASHFS_SYMLINK_TYPE;
	*/
	
	lnk->base.encoded_type = SQUASHFS_SYMLINK_TYPE;
	
	size_t namelen = strlen(lnk->name);
	
	if (lnk->base.encoded_type == SQUASHFS_SYMLINK_TYPE) {
		struct squashfs_symlink_inode_header hdr;
	
		hdr.inode_type = cpu_to_le16(lnk->base.encoded_type);
		hdr.mode = cpu_to_le16(lnk->base.attr->mode);
		hdr.uid = cpu_to_le16(lnk->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(lnk->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(lnk->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(lnk->base.inode_number);
		hdr.nlink = cpu_to_le32(lnk->base.nlink);
		
		hdr.symlink_size = cpu_to_le32(namelen);
		
		if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &lnk->base.inode_table_entry)) return false;
	}
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

const libsqfs_inode_vmt libsqfs_symlink_inode_vmt = {
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
	
	lnk->base.vmt = &libsqfs_symlink_inode_vmt;
	libsqfs_inode_init(image, &lnk->base, attr);
	
	return lnk;
}

libsqfs_inode_t
libsqfs_symlink_inode_downcast(libsqfs_symlink_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}

struct _libsqfs_device_inode {
	struct _libsqfs_inode base;
	
	char type;
	unsigned int major, minor;
};

static bool
libsqfs_device_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_device_inode_t dev = (libsqfs_device_inode_t) inode;
	libsqfs_image_t image = dev->base.image;
	
	if (!dev->base.attr->xattrset) {
		struct squashfs_dev_inode_header hdr;
		
		if (dev->type == 'c')
			dev->base.encoded_type = SQUASHFS_CHRDEV_TYPE;
		else
			dev->base.encoded_type = SQUASHFS_BLKDEV_TYPE;
		
		hdr.inode_type = cpu_to_le16(dev->base.encoded_type);
		hdr.mode = cpu_to_le16(dev->base.attr->mode);
		hdr.uid = cpu_to_le16(dev->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(dev->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(dev->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(dev->base.inode_number);
		hdr.nlink = cpu_to_le32(dev->base.nlink);
		
		hdr.rdev = cpu_to_le32(
			(dev->major << 8) | (dev->minor & 0xff) | ((dev->minor & ~0xff) << 12)
		);
		
		if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &dev->base.inode_table_entry)) return false;
	} else {
		struct squashfs_ldev_inode_header hdr;
		
		if (dev->type == 'c')
			dev->base.encoded_type = SQUASHFS_LCHRDEV_TYPE;
		else
			dev->base.encoded_type = SQUASHFS_LBLKDEV_TYPE;
		
		hdr.inode_type = cpu_to_le16(dev->base.encoded_type);
		hdr.mode = cpu_to_le16(dev->base.attr->mode);
		hdr.uid = cpu_to_le16(dev->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(dev->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(dev->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(dev->base.inode_number);
		hdr.nlink = cpu_to_le32(dev->base.nlink);
		
		hdr.rdev = cpu_to_le32(
			(dev->major << 8) | (dev->minor & 0xff) | ((dev->minor & ~0xff) << 12)
		);
		
		hdr.xattr = cpu_to_le32(dev->base.attr->xattrset->id);
		
		if (!libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &dev->base.inode_table_entry)) return false;
	}
	
	return true;
}

static void
libsqfs_device_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_device_inode_t dev = (libsqfs_device_inode_t) inode;
	free(dev);
}

const libsqfs_inode_vmt libsqfs_device_inode_vmt = {
	.serialize = &libsqfs_device_inode_serialize,
	.destroy = &libsqfs_device_inode_destroy
};

libsqfs_device_inode_t
libsqfs_device_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, char type, unsigned int major, unsigned int minor)
{
	if ((type != 'c') && (type != 'b')) {
		libsqfs_image_flag_error(image, "Unknown device type", false);
		return 0;
	}
	libsqfs_device_inode_t dev = malloc(sizeof(*dev));
	if (!dev) {
		libsqfs_image_out_of_memory(image, false);
		return 0;
	}
	
	dev->base.vmt = &libsqfs_device_inode_vmt;
	libsqfs_inode_init(image, &dev->base, attr);
	dev->major = major;
	dev->minor = minor;
	dev->type = type;
	
	return dev;
}

libsqfs_inode_t
libsqfs_device_inode_downcast(libsqfs_device_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}

struct _libsqfs_fifo_inode {
	struct _libsqfs_inode base;
};

static bool
libsqfs_fifo_inode_serialize(libsqfs_inode_t inode)
{
	libsqfs_fifo_inode_t fifo = (libsqfs_fifo_inode_t) inode;
	libsqfs_image_t image = fifo->base.image;

	if (fifo->base.attr->xattrset)
		fifo->base.encoded_type = SQUASHFS_LFIFO_TYPE;
	else
		fifo->base.encoded_type = SQUASHFS_FIFO_TYPE;
	
	if (fifo->base.encoded_type == SQUASHFS_FIFO_TYPE) {
		struct squashfs_ipc_inode_header hdr;
		
		hdr.inode_type = cpu_to_le16(fifo->base.encoded_type);
		hdr.mode = cpu_to_le16(fifo->base.attr->mode);
		hdr.uid = cpu_to_le16(fifo->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(fifo->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(fifo->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(fifo->base.inode_number);
		hdr.nlink = cpu_to_le32(fifo->base.nlink);
		
		return libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &fifo->base.inode_table_entry);
	} else {
		struct squashfs_lipc_inode_header hdr;
		
		hdr.inode_type = cpu_to_le16(fifo->base.encoded_type);
		hdr.mode = cpu_to_le16(fifo->base.attr->mode);
		hdr.uid = cpu_to_le16(fifo->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(fifo->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(fifo->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(fifo->base.inode_number);
		hdr.nlink = cpu_to_le32(fifo->base.nlink);
		if (fifo->base.attr->xattrset)
			hdr.xattr = cpu_to_le32(fifo->base.attr->xattrset->id);
		else
			hdr.xattr = cpu_to_le32(-1);
		
		return libsqfs_metatable_append(&image->inode_table, &hdr, sizeof(hdr), &fifo->base.inode_table_entry);
	}
}

static void
libsqfs_fifo_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_fifo_inode_t fifo = (libsqfs_fifo_inode_t) inode;
	free(fifo);
}

const libsqfs_inode_vmt libsqfs_fifo_inode_vmt = {
	.serialize = &libsqfs_fifo_inode_serialize,
	.destroy = &libsqfs_fifo_inode_destroy
};

libsqfs_fifo_inode_t
libsqfs_fifo_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr)
{
	libsqfs_fifo_inode_t fifo = malloc(sizeof(*fifo));
	
	if (!fifo) {
		libsqfs_image_out_of_memory(image, false);
		return 0;
	}

	fifo->base.vmt = &libsqfs_fifo_inode_vmt;
	libsqfs_inode_init(image, &fifo->base, attr);
	
	return fifo;
}

libsqfs_inode_t
libsqfs_fifo_inode_downcast(libsqfs_fifo_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}
