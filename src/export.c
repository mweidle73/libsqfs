#include "internal.h"
#include "squashfs_fs.h"

void
libsqfs_export_table_init(libsqfs_export_table * export_tab)
{
	export_tab->offset = -1;
}

bool
libsqfs_export_table_write(libsqfs_image_t image, libsqfs_export_table * export_tab)
{
	uint64_t inode_map[image->inodes.count];
	
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		inode_map[inode->inode_number-1] = cpu_to_le64(inode->squashfs_inode);
		inode = inode->next;
	}
	
	export_tab->offset = libsqfs_write_metatable(image, inode_map, sizeof(inode_map), true, true);
	
	return export_tab->offset != -1;
}
