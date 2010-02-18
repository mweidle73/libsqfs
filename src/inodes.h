#ifndef LIBSQFS_INODES_H
#define LIBSQFS_INODES_H

#include <libsqfs.h>

/* inodes */

struct _libsqfs_inodeattr {
	libsqfs_inodeattr_t prev, next;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t ctime;
	
	uint16_t mapped_uid, mapped_gid;
};

void
libsqfs_inodeattr_destroy(libsqfs_inodeattr_t attr);

typedef struct _libsqfs_inode_vmt libsqfs_inode_vmt;

struct _libsqfs_inode_vmt {
	void (*destroy)(libsqfs_inode_t inode);
	size_t (*encoded_size)(libsqfs_inode_t inode);
	void (*encode)(libsqfs_inode_t inode, void * dst);
};

#define LIBSQFS_INODE_COMMON \
	const libsqfs_inode_vmt * vmt; \
	libsqfs_inode_t prev, next; \
	libsqfs_image_t image; \
	\
	libsqfs_inodeattr_t attr; \
	size_t nlink; \
	\
	size_t inode_number; \
	long long squashfs_inode; \
	int encoded_type; \

struct _libsqfs_inode {
	LIBSQFS_INODE_COMMON
};

void
libsqfs_inode_init(libsqfs_image_t image, libsqfs_inode_t inode);

void
libsqfs_inode_destroy(libsqfs_inode_t inode);

typedef struct _libsqfs_directory_entry libsqfs_directory_entry;

struct _libsqfs_directory_inode {
	LIBSQFS_INODE_COMMON
	
	libsqfs_directory_inode_t parent;
	struct {
		libsqfs_directory_entry * first, * last;
	} entries;
	
	libsqfs_directory_inode_t prev_dir, next_dir;
	
	libsqfs_off_t dir_table_offset;
};

typedef struct _libsqfs_inode_table {
	libsqfs_off_t offset;
	libsqfs_off_t size;
} libsqfs_inode_table;

void
libsqfs_inode_table_layout(libsqfs_image_t image, libsqfs_inode_table * inode_table);

bool
libsqfs_inode_table_write(libsqfs_image_t image, libsqfs_inode_table * inode_table);

#endif
