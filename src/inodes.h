#ifndef LIBSQFS_INODES_H
#define LIBSQFS_INODES_H

#include <libsqfs.h>

#include "metatable.h"
#include "compressor.h"

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
	bool (*serialize)(libsqfs_inode_t inode);
	/*size_t (*encoded_size)(libsqfs_inode_t inode);
	void (*encode)(libsqfs_inode_t inode, void * dst);*/
};

#define LIBSQFS_INODE_COMMON \
	const libsqfs_inode_vmt * vmt; \
	libsqfs_inode_t prev, next; \
	libsqfs_image_t image; \
	\
	libsqfs_inodeattr_t attr; \
	size_t nlink; \
	\
	libsqfs_metatable_entry inode_table_entry; \
	unsigned int inode_number; \
	int encoded_type; \

struct _libsqfs_inode {
	LIBSQFS_INODE_COMMON
};

void
libsqfs_inode_init(libsqfs_image_t image, libsqfs_inode_t inode);

void
libsqfs_inode_destroy(libsqfs_inode_t inode);

static inline long long
libsqfs_encoded_inode(const libsqfs_inode_t inode)
{
	return inode->inode_table_entry.offset | (((long long)inode->inode_table_entry.block) << 16);
}

typedef struct _libsqfs_directory_entry libsqfs_directory_entry;

struct _libsqfs_directory_inode {
	LIBSQFS_INODE_COMMON
	
	libsqfs_directory_inode_t parent;
	struct {
		libsqfs_directory_entry * first, * last;
	} entries;
	
	size_t indexed_count;
	libsqfs_directory_entry * first_indexed;
	
	/* location of entry list of this directory within directory table */
	libsqfs_metatable_entry dir_table_entry;
	/* size of encoded directory entry listing */
	size_t encoded_entries_size;
};

/* recursively serialize inodes to disk */
bool
libsqfs_inode_serialize(libsqfs_inode_t inode);

#endif
