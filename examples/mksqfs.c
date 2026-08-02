/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2010
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
 * Stephen Hemminger <stephen.hemminger@vyatta.com>
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

#include <libsqfs.h>

#include <errno.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/xattr.h>

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

static libsqfs_regular_inode_t
add_file(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	libsqfs_data_t data = libsqfs_data_create_from_file(image, pathname);
	libsqfs_regular_inode_t file = libsqfs_regular_inode_create(image, attr, data);
	
	return file;
}

static libsqfs_symlink_inode_t
add_symlink(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	char target[1024];
	int len = readlink(pathname, target, sizeof(target)-1);
	target[len] = 0;
	return libsqfs_symlink_inode_create(image, attr, target);
}

static libsqfs_fifo_inode_t
add_fifo(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	return libsqfs_fifo_inode_create(image, attr);
}

static libsqfs_device_inode_t
add_device(libsqfs_image_t image, const char pathname[], libsqfs_inodeattr_t attr)
{
	struct stat st;
	lstat(pathname, &st);
	if (S_ISCHR(st.st_mode))
		return libsqfs_device_inode_create(image, attr, 'c', major(st.st_rdev), minor(st.st_rdev));
	else
		return libsqfs_device_inode_create(image, attr, 'b', major(st.st_rdev), minor(st.st_rdev));
}


static libsqfs_xattrset_t
get_xattrset(libsqfs_image_t image, const char *path)
{
	ssize_t xattr_size = listxattr(path, NULL, 0);
	if (xattr_size <= 0)
		return NULL;

	char xattr_buf[xattr_size + 1];
	xattr_size = listxattr(path, xattr_buf, xattr_size+1);

	const char *name = xattr_buf;
	size_t name_len;
	size_t bufsize = 128;
	void *buf = malloc(bufsize);
	
	libsqfs_xattr_t * attrs = 0;
	size_t nattrs = 0;

	do {
		ssize_t val_len;
	retry:
		val_len = getxattr(path, name, buf, bufsize);
		if (val_len < 0) {
			if (errno == ERANGE) {
				bufsize *= 2;
				buf = realloc(buf, bufsize);
				goto retry;
			}

			fprintf(stderr, "%s: getxattr(%s) failed: %s\n",
				path, name, strerror(errno));
			abort();
		}

		attrs = realloc(attrs, (nattrs + 1) * sizeof(attrs[0]));
		attrs[nattrs++] = libsqfs_xattr_create(image, name, val_len, buf);

		name_len = strlen(name);
		name += name_len + 1;
	} while ((xattr_size -= name_len + 1) > 0);
	
	libsqfs_xattrset_t xattrset = libsqfs_xattrset_create(image, nattrs, attrs);
	
	free(buf);
	free(attrs);
	return xattrset;
}

static libsqfs_inodeattr_t
get_inode_attrs(libsqfs_image_t image, const struct stat * st, const char * path)
{
	libsqfs_xattrset_t xattrset = get_xattrset(image, path);
	return libsqfs_inodeattr_create_extended(image,
		st->st_uid, st->st_gid,
		st->st_mode & 0777, st->st_ctime, xattrset);
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
	
	dir_entry_name * first = 0, *current;
	
	/* scan directory, sort entries, and treat files first */
	DIR * srcdir = opendir(pathname);
	struct dirent * entry;
	while ( (entry = readdir(srcdir)) != 0) {
		if (strcmp(entry->d_name, ".") == 0) continue;
		if (strcmp(entry->d_name, "..") == 0) continue;
		
		current = malloc(sizeof(*current));
		strcpy(current->name, entry->d_name);
		int path_len = snprintf(current->path, sizeof(current->path),
			"%s/%s", pathname, entry->d_name);
		if (path_len < 0 || (size_t) path_len >= sizeof(current->path)) {
			fprintf(stderr, "Path is too long: %s/%s\n",
				pathname, entry->d_name);
			exit(1);
		}
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
	}
		
	closedir(srcdir);
	current = first;
	while(current) {
		if (S_ISREG(current->st.st_mode)) {
			libsqfs_inode_t inode = inode_cache_lookup(current->st.st_dev, current->st.st_ino);
			if (!inode) {
				attr = get_inode_attrs(image, &current->st, current->path);
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
			libsqfs_inode_t sub;

			attr = get_inode_attrs(image, &current->st, current->path);
			if (S_ISDIR(current->st.st_mode))
				sub = libsqfs_directory_inode_downcast(add_directory(image, current->path, attr));
			else if (S_ISLNK(current->st.st_mode))
				sub = libsqfs_symlink_inode_downcast(add_symlink(image, current->path, attr));
			else if (S_ISBLK(current->st.st_mode) || 
				 S_ISCHR(current->st.st_mode))
				sub = libsqfs_device_inode_downcast(add_device(image, current->path, attr));
			else if (S_ISFIFO(current->st.st_mode))
				sub = libsqfs_fifo_inode_downcast(add_fifo(image, current->path, attr));
			else {
				fprintf(stderr, "Don't know how to handle %s %#x\n",
					current->path, current->st.st_mode);
				exit(1);
			}
			
			libsqfs_directory_add_entry(dir, current->name, sub);
		}
		dir_entry_name * next = current->next;
		free(current);
		current = next;
	}
	
	return dir;
}

libsqfs_compressor_t
parse_compressor(const char * descr)
{
	libsqfs_compressor_t compressor = 0;
	
	if (strcmp(descr, "null") == 0) {
		compressor = libsqfs_compressor_copy(&libsqfs_compressor_null);
	}
#ifdef LIBSQFS_HAVE_COMPRESSOR_ZLIB
	else if (strcmp(descr, "zlib") == 0) {
		compressor = libsqfs_compressor_copy(&libsqfs_compressor_zlib);
	}
#endif
#ifdef LIBSQFS_HAVE_COMPRESSOR_LZMA
	else if (strcmp(descr, "lzma") == 0) {
		compressor = libsqfs_compressor_copy(&libsqfs_compressor_lzma);
	} else if (strcmp(descr, "xz") == 0) {
		compressor = libsqfs_compressor_xz_create_default();
	}
#endif
	
	if (!compressor) {
		fprintf(stderr, "Invalid compressor: '%s'\n", descr);
		exit(1);
	}
	
	return compressor;
}

static void
parse_options(int argc, char ** argv, libsqfs_image_options_t options, char ** src, char ** dst)
{
	static const struct option long_options[] = {
		{"compressor", 1, 0, 'c'},
		{0, 0, 0, 0}
	};
	
	for(;;) {
		int index;
		int opt = getopt_long(argc, argv, "c:", long_options, &index);
		if (opt == -1) break;
		switch(opt) {
			case 'c': {
				libsqfs_compressor_t compressor = parse_compressor(optarg);
				libsqfs_image_options_set_compressor(options, compressor);
				libsqfs_compressor_destroy(compressor);
			}
			default: break;
		}
	}
	
	if (optind > argc - 2) {
		fprintf(stderr, "Usage: %s [options] [source directory] [destination image]\n", argv[0]);
		exit(1);
	}
	
	*src = argv[optind];
	*dst = argv[optind + 1];
}

int main(int argc, char ** argv)
{
	libsqfs_image_options_t options = libsqfs_image_options_create();
	
	char * src = 0, * dst = 0;
	parse_options(argc, argv, options, &src, &dst);
	
	libsqfs_destination_t dest = libsqfs_destination_create_for_file(dst, 0644);
	libsqfs_image_t image = libsqfs_image_create(dest, options);
	libsqfs_image_options_destroy(options);
	
	libsqfs_image_auto_spawn_threads(image);
	
	libsqfs_inodeattr_t attr = libsqfs_inodeattr_create_simple(image, 0, 0, 0755, 0);
	libsqfs_directory_inode_t root = add_directory(image, src, attr);
	
	libsqfs_image_set_root(image, root);
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
