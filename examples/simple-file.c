#include <libsqfs.h>

const char sentence[] = "The quick brown fox jumps over the lazy dog.\n";
const char another[] = "Another sentence.\n";

int main(void)
{
	libsqfs_destination_t dest = libsqfs_destination_create_for_file("/tmp/test.img", 0644);
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_inodeattr_t fileattr = libsqfs_inodeattr_create_simple(image, 1000, 1000, 0644, 0);
	libsqfs_data_t data1 = libsqfs_data_create_for_static_buffer(image, sentence, sizeof(sentence)-1);
	libsqfs_regular_inode_t file1 = libsqfs_regular_inode_create(image, fileattr, data1);
	libsqfs_data_t data2 = libsqfs_data_create_for_static_buffer(image, another, sizeof(another)-1);
	libsqfs_regular_inode_t file2 = libsqfs_regular_inode_create(image, fileattr, data2);
	
	libsqfs_inodeattr_t dirattr = libsqfs_inodeattr_create_simple(image, /* uid */ 1000, /* gid */ 1000, 0755, 0);
	libsqfs_directory_inode_t rootdir = libsqfs_directory_inode_create(image, dirattr);
	libsqfs_image_set_root(image, rootdir);
	
	libsqfs_directory_add_entry(rootdir, "sentence.txt", libsqfs_regular_inode_downcast(file1));
	libsqfs_directory_add_entry(rootdir, "z.txt", libsqfs_regular_inode_downcast(file2));
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
