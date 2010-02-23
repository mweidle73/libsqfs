#include <libsqfs.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

libsqfs_regular_inode_t
add_file(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	libsqfs_data_t data = libsqfs_data_create_from_file(image, pathname);
	libsqfs_regular_inode_t file = libsqfs_regular_inode_create(image, attr, data);
	
	return file;
}

libsqfs_symlink_inode_t
add_symlink(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	char target[1024];
	readlink(pathname, target, sizeof(target)-1);
	return libsqfs_symlink_inode_create(image, attr, target);
}

libsqfs_directory_inode_t
add_directory(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	fprintf(stderr, "Add directory %s\n", pathname);
	libsqfs_directory_inode_t dir = libsqfs_directory_inode_create(image, attr);
	DIR * srcdir = opendir(pathname);
	struct dirent * entry;
	while ( (entry = readdir(srcdir)) != 0) {
		if (strcmp(entry->d_name, ".") == 0) continue;
		if (strcmp(entry->d_name, "..") == 0) continue;
		
		char tmpname[PATH_MAX + 1];
		strncpy(tmpname, pathname, sizeof(tmpname));
		strncat(tmpname, "/", sizeof(tmpname));
		strncat(tmpname, entry->d_name, sizeof(tmpname));
		
		struct stat st;
		lstat(tmpname, &st);
		if (!S_ISREG(st.st_mode) && !S_ISDIR(st.st_mode) && !S_ISLNK(st.st_mode)) abort();
		
		attr = libsqfs_inodeattr_create_simple(image, st.st_uid, st.st_gid, st.st_mode & 0777, st.st_ctime);
		libsqfs_inode_t sub = 0;
		if (S_ISREG(st.st_mode)) sub = libsqfs_regular_inode_downcast(add_file(image, tmpname, attr));
		else if (S_ISDIR(st.st_mode)) sub = libsqfs_directory_inode_downcast(add_directory(image, tmpname, attr));
		else if (S_ISLNK(st.st_mode)) sub = libsqfs_symlink_inode_downcast(add_symlink(image, tmpname, attr));
		
		libsqfs_directory_add_entry(dir, entry->d_name, sub);
	}
	closedir(srcdir);
	
	return dir;
}

int main(int argc, char ** argv)
{
	if (argc<3) {
		fprintf(stderr, "Usage: %s [destination image] [source directory]\n", argv[0]);
		exit(1);
	}
	libsqfs_destination_t dest = libsqfs_destination_create_for_file(argv[1], 0644);
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_inodeattr_t attr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	libsqfs_directory_inode_t root = add_directory(image, argv[2], attr);
	
	libsqfs_image_set_root(image, root);
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
