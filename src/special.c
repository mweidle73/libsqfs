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
	
	hdr.symlink_size = cpu_to_le32(namelen);
	
	if (!libsqfs_metatable_append(&image->inode_table.tab, &hdr, sizeof(hdr), &lnk->inode_table_entry)) return false;
	libsqfs_metatable_entry dummy;
	if (!libsqfs_metatable_append(&image->inode_table.tab, lnk->name, namelen, &dummy)) return false;
	
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
	/* FIXME: flag error on image */
	if (!lnk) return 0;
	
	lnk->name = strdup(name);
	if (!lnk->name) {
		free(lnk);
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
