#include <libsqfs.h>

int main(void)
{
	libsqfs_destination_t dest = libsqfs_destination_create_for_file("/tmp/test.img", 0644);
	libsqfs_image_t image = libsqfs_image_create(dest);
	
	libsqfs_inodeattr_t iattr = libsqfs_inodeattr_create_simple(image, /* uid */ 1000, /* gid */ 1000, 0755, 0);
	
	libsqfs_directory_inode_t abc = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t def = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t ghi = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t jkl = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t mno = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t pqr = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t stu = libsqfs_directory_inode_create(image, iattr);
	libsqfs_directory_inode_t vwx = libsqfs_directory_inode_create(image, iattr);
	
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, iattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_directory_add_entry(rootdir, "abc", libsqfs_directory_inode_downcast(abc));
	libsqfs_directory_add_entry(rootdir, "def", libsqfs_directory_inode_downcast(def));
	libsqfs_directory_add_entry(rootdir, "ghi", libsqfs_directory_inode_downcast(ghi));
	libsqfs_directory_add_entry(rootdir, "jkl", libsqfs_directory_inode_downcast(jkl));
	libsqfs_directory_add_entry(rootdir, "mno", libsqfs_directory_inode_downcast(mno));
	libsqfs_directory_add_entry(rootdir, "pqr", libsqfs_directory_inode_downcast(pqr));
	libsqfs_directory_add_entry(rootdir, "stu", libsqfs_directory_inode_downcast(stu));
	libsqfs_directory_add_entry(rootdir, "vwx", libsqfs_directory_inode_downcast(vwx));
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
