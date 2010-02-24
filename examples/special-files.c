#include <libsqfs.h>

int main(void)
{
	libsqfs_destination_t dest = libsqfs_destination_create_for_file("/tmp/test.img", 0644);
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_inodeattr_t fileattr = libsqfs_inodeattr_create_simple(image, 0, 0, 0600, 0);
	libsqfs_inodeattr_t dirattr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, dirattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_device_inode_t chrdev, blkdev;
	chrdev = libsqfs_device_inode_create(image, fileattr, 'c', 4, 0);
	blkdev = libsqfs_device_inode_create(image, fileattr, 'b', 2, 0);
	libsqfs_symlink_inode_t symlink = libsqfs_symlink_inode_create(image, fileattr, "fd0");
	
	libsqfs_directory_add_entry(rootdir, "tty0", libsqfs_device_inode_downcast(chrdev));
	libsqfs_directory_add_entry(rootdir, "fd0", libsqfs_device_inode_downcast(blkdev));
	libsqfs_directory_add_entry(rootdir, "floppy", libsqfs_symlink_inode_downcast(symlink));
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
