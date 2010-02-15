#include <libsqfs.h>

int main(void)
{
	libsqfs_destination_t dest = libsqfs_destination_create_for_file("/tmp/test.img", 0644);
	libsqfs_image_t image = libsqfs_image_create(dest);
	
	libsqfs_inodeattr_t iattr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, iattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
