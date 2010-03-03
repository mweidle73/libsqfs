#include <libsqfs.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

typedef struct inode_cache_entry inode_cache_entry;

struct inode_cache_entry {
	dev_t dev;
	ino_t ino;
	libsqfs_inode_t inode;
	inode_cache_entry * next;
};

#define INODE_HASH_SIZE 1024
static inode_cache_entry * inode_cache[INODE_HASH_SIZE] = {};

static inode_cache_entry ** inode_cache_bucket(dev_t dev, ino_t ino)
{
	size_t hash = dev + ino;
	return &inode_cache[hash % INODE_HASH_SIZE];
}

static libsqfs_inode_t
inode_cache_lookup(dev_t dev, ino_t ino)
{
	inode_cache_entry * tmp = * inode_cache_bucket(dev, ino);
	while(tmp) {
		if (tmp->dev == dev && tmp->ino == ino) return tmp->inode;
		tmp = tmp->next;
	}
	return 0;
}

static void
inode_cache_insert(dev_t dev, ino_t ino, libsqfs_inode_t inode)
{
	inode_cache_entry * tmp = malloc(sizeof(*tmp));
	inode_cache_entry ** bucket = inode_cache_bucket(dev, ino);
	tmp->dev = dev;
	tmp->ino = ino;
	tmp->inode = inode;
	tmp->next = *bucket;
	*bucket = tmp;
}

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

typedef struct dir_entry_name dir_entry_name;
struct dir_entry_name {
	dir_entry_name * prev, * next;
	char name[PATH_MAX+1], path[PATH_MAX+1];
	struct stat st;
};

libsqfs_directory_inode_t
add_directory(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	fprintf(stderr, "Add directory %s\n", pathname);
	libsqfs_directory_inode_t dir = libsqfs_directory_inode_create(image, attr);
	
	dir_entry_name * first = 0, * last = 0, *current;
	
	/* scan directory, sort entries, and treat files first */
	DIR * srcdir = opendir(pathname);
	struct dirent * entry;
	while ( (entry = readdir(srcdir)) != 0) {
		if (strcmp(entry->d_name, ".") == 0) continue;
		if (strcmp(entry->d_name, "..") == 0) continue;
		
		current = malloc(sizeof(*current));
		strcpy(current->name, entry->d_name);
		strncpy(current->path, pathname, sizeof(current->name));
		strncat(current->path, "/", sizeof(current->name));
		strncat(current->path, entry->d_name, sizeof(current->name));
		lstat(current->path, &current->st);
		
		dir_entry_name * insert_before = first, * insert_after = 0;
		while (insert_before && strcmp(current->name, insert_before->name)>0) {
			insert_after = insert_before;
			insert_before = insert_before->next;
		}
		
		current->prev = insert_after;
		current->next = insert_before;
		
		if (insert_after) insert_after->next = current;
		else first = current;
		if (insert_before) insert_before->prev = current;
		else last = current;
	}
		
	closedir(srcdir);
	current = first;
	while(current) {
		if (S_ISREG(current->st.st_mode)) {
			libsqfs_inode_t inode = inode_cache_lookup(current->st.st_dev, current->st.st_ino);
			if (!inode) {
				attr = libsqfs_inodeattr_create_simple(image, current->st.st_uid, current->st.st_gid, current->st.st_mode & 0777, current->st.st_ctime);
				
				inode = libsqfs_regular_inode_downcast(add_file(image, current->path, attr));
				inode_cache_insert(current->st.st_dev, current->st.st_ino, inode);
			} 
			
			libsqfs_directory_add_entry(dir, current->name, inode);
		}
		current = current->next;
	}
	
	current = first;
	while(current) {
		if (!S_ISREG(current->st.st_mode)) {
			attr = libsqfs_inodeattr_create_simple(image, current->st.st_uid, current->st.st_gid, current->st.st_mode & 0777, current->st.st_ctime);
			if (!S_ISDIR(current->st.st_mode) && !S_ISLNK(current->st.st_mode)) abort();
			
			libsqfs_inode_t sub = 0;
			if (S_ISDIR(current->st.st_mode)) sub = libsqfs_directory_inode_downcast(add_directory(image, current->path, attr));
			else if (S_ISLNK(current->st.st_mode)) sub = libsqfs_symlink_inode_downcast(add_symlink(image, current->path, attr));
			
			libsqfs_directory_add_entry(dir, current->name, sub);
		}
		dir_entry_name * next = current->next;
		free(current);
		current = next;
	}
	
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
	libsqfs_image_auto_spawn_threads(image);
	
	libsqfs_inodeattr_t attr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	libsqfs_directory_inode_t root = add_directory(image, argv[2], attr);
	
	libsqfs_image_set_root(image, root);
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
