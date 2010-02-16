#include "internal.h"
#include "squashfs_fs.h"

bool
libsqfs_write_superblock(libsqfs_image_t image)
{
	struct squashfs_super_block sb;
	
	sb.s_magic = cpu_to_le32(SQUASHFS_MAGIC);
	sb.inodes = cpu_to_le32(image->inodes.count);
	sb.mkfs_time = cpu_to_le32(image->creation_time);
	sb.block_size = cpu_to_le32(image->block_size);
	sb.fragments = cpu_to_le32(0 /* FIXME: no fragments yet */);
	sb.compression = cpu_to_le16(image->compression_method);
	sb.block_log = cpu_to_le16(image->block_size_log);
	sb.flags = cpu_to_le16(0x4b /* FIXME: flags are not correct yet */);
	sb.no_ids = cpu_to_le16(image->idtable.nids);
	sb.s_major = cpu_to_le16(4);
	sb.s_minor = cpu_to_le16(0);
	
	sb.root_inode = cpu_to_le64(image->root->inode_table_offset);
	sb.bytes_used = cpu_to_le64(image->size);
	sb.id_table_start = cpu_to_le64(image->idtable.offset);
	sb.xattr_table_start = cpu_to_le64(-1 /* FIXME: no xattrs yet */);
	sb.inode_table_start = cpu_to_le64(image->inode_table.offset);
	sb.directory_table_start = cpu_to_le64(image->dir_table.offset);
	sb.fragment_table_start = cpu_to_le64(image->frag_table.offset);
	sb.lookup_table_start = cpu_to_le64(-1 /* FIXME: no nfs export table yet */);
	
	ssize_t count = libsqfs_pwrite(image->dst, &sb, sizeof(sb), 0);
	return count == sizeof(sb);
}

void
libsqfs_reserve_superblock(libsqfs_image_t image)
{
	libsqfs_image_reserve(image, sizeof(struct squashfs_super_block));
}

