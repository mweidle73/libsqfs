#include "internal.h"

#include <string.h>

#include "squashfs_fs.h"

/*
	structure of directory table:
	
	[ squashfs_dir_header squashfs_dir_entry* ]*
	
	Each header encodes the number of entries to follow, one inode block
	and one "base" inode number; each following entry encodes the offset
	of the inode within the block and the difference to the "base" inode
	number to the inode number of the entry.
	
	A new header must inserted if
	- inodes of following dir entries are in a different block
	- inode numbers differ by more than +32767/-32768
	- 256 entries have been coded already
	- number of bytes for header + its entries would exceed METADATA_SIZE
*/

struct _libsqfs_directory_entry {
	libsqfs_directory_entry * prev, * next;
	char * name;
	libsqfs_inode_t inode;
	
	size_t index, offset, size;
};

/* the following function takes as "reference" the first entry after the
header coded last; it may be NULL if no header has been coded */
static bool
need_new_dir_header(libsqfs_directory_entry * entry, libsqfs_directory_entry * reference)
{
	if (!reference) return true;
	if ((entry->inode->squashfs_inode>>16) != (reference->inode->squashfs_inode>>16))
		return true;
	ssize_t diff = (ssize_t)entry->inode->inode_number - (ssize_t)reference->inode->inode_number;
	if (diff<-32768 || diff>32767) return true;
	
	if (entry->size + entry->offset - reference->offset > SQUASHFS_METADATA_SIZE)
		return true;
	
	if (entry->index - reference->index > 255)
		return true;
	
	return false;
}

static size_t
libsqfs_directory_encoded_size(libsqfs_directory_inode_t dir)
{
	size_t offset = 0, index=0;
	libsqfs_directory_entry * entry = dir->entries.first, * reference = 0;
	while(entry) {
		entry->index = index++;
		entry->offset = offset;
		entry->size = sizeof(struct squashfs_dir_entry) + strlen(entry->name);
		if (need_new_dir_header(entry, reference)) {
			offset += sizeof(struct squashfs_dir_header);
			reference = entry;
		}
		offset += entry->size;
		entry = entry->next;
	}
	
	return offset;
}

static void
libsqfs_directory_encode(libsqfs_directory_inode_t dir, void * dst)
{
	libsqfs_directory_entry * entry = dir->entries.first, * reference = 0;
	/* the "count" of entries after a header must be recorded into the
	header; do this by revisiting the header when deciding to write
	the next header */
	uint32_t * entry_count_loc = 0, count = 0;
	while(entry) {
		if (need_new_dir_header(entry, reference)) {
			reference = entry;
			struct squashfs_dir_header * hdr = dst;
			dst = hdr + 1;
			
			if (entry_count_loc) *entry_count_loc = cpu_to_le32(count-1);
			entry_count_loc = &hdr->count;
			count = 0;
			
			hdr->start_block = cpu_to_le32(entry->inode->squashfs_inode>>16);
			hdr->inode_number = cpu_to_le32(entry->inode->inode_number);
		}
		
		struct squashfs_dir_entry * ent = dst;
		dst = ent + 1;
		ent->offset = cpu_to_le16(entry->inode->squashfs_inode & 0xffff);
		ent->inode_number = cpu_to_le16(entry->inode->inode_number - reference->inode->inode_number);
		ent->type = cpu_to_le16(entry->inode->encoded_type);
		size_t namelen = strlen(entry->name);
		ent->size = cpu_to_le16(namelen - 1);
		
		memcpy(dst, entry->name, namelen);
		dst = namelen + (char *)dst;
		
		count++;
		
		entry = entry->next;
	}
	if (entry_count_loc) *entry_count_loc = cpu_to_le32(count-1);
}

static void
libsqfs_directory_inode_destroy(libsqfs_inode_t inode)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	/* FIXME: free entries */
	free(dir);
}

static size_t
libsqfs_directory_inode_encoded_size(libsqfs_inode_t inode)
{
	return sizeof(struct squashfs_dir_inode_header);
}

static void
libsqfs_directory_inode_encode(libsqfs_inode_t inode, void * dst)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	struct squashfs_dir_inode_header * hdr = dst;
	
	hdr->inode_type = cpu_to_le16(SQUASHFS_DIR_TYPE);
	hdr->mode = cpu_to_le16(dir->attr->mode);
	hdr->uid = cpu_to_le16(dir->attr->mapped_uid);
	hdr->guid = cpu_to_le16(dir->attr->mapped_gid);
	hdr->mtime = cpu_to_le32(dir->attr->ctime);
	hdr->inode_number = cpu_to_le32(dir->inode_number);
	
	hdr->start_block = cpu_to_le32(dir->dir_table_offset / SQUASHFS_METADATA_SIZE);
	hdr->offset = cpu_to_le16(dir->dir_table_offset % SQUASHFS_METADATA_SIZE);
	
	hdr->nlink = cpu_to_le32(dir->nlink);
	
	hdr->file_size = cpu_to_le16(libsqfs_directory_encoded_size(dir) + 3);
	
	if (dir->parent)
		hdr->parent_inode = cpu_to_le32(dir->parent->inode_number);
	else
		hdr->parent_inode = cpu_to_le32(dir->image->inodes.count+1);
		/* this is the "invalid inode marker" */
}

const libsqfs_inode_vmt libsqfs_directory_inode_vmt = {
	.destroy = &libsqfs_directory_inode_destroy,
	.encoded_size = &libsqfs_directory_inode_encoded_size,
	.encode = &libsqfs_directory_inode_encode
};

libsqfs_directory_inode_t
libsqfs_directory_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr)
{
	libsqfs_directory_inode_t dir = malloc(sizeof(*dir));
	if (!dir) return 0;
	
	dir->vmt = &libsqfs_directory_inode_vmt;
	dir->attr = attr;
	dir->nlink = 1;
	dir->entries.first = dir->entries.last = 0;
	dir->encoded_type = SQUASHFS_DIR_TYPE;
	libsqfs_inode_init(image, (libsqfs_inode_t) dir);
	
	dir->prev_dir = image->dir_table.dirs.last;
	dir->next_dir = 0;
	if (image->dir_table.dirs.last) image->dir_table.dirs.last->next_dir = dir;
	else image->dir_table.dirs.first = dir;
	image->dir_table.dirs.last = dir;
	return dir;
}

bool
libsqfs_directory_add_entry(libsqfs_directory_inode_t parent, const char * name, libsqfs_inode_t inode)
{
	/* FIXME: on failed memory allocation, set error flag on image */
	libsqfs_directory_entry * entry = malloc(sizeof(*entry));
	if (!entry) return false;
	
	entry->name = strdup(name);
	if (!entry->name) {
		free(entry);
		return false;
	}
	
	if (inode->vmt == &libsqfs_directory_inode_vmt) {
		if (((libsqfs_directory_inode_t) inode)->parent) {
			/* this directory already has a parent, it cannot
			have another one; bail out */
			free(entry->name);
			free(entry);
			return false;
		}
		((libsqfs_directory_inode_t) inode)->parent = parent;
		parent->nlink++;
	}
	
	inode->nlink++;
	entry->inode = inode;
	entry->prev = parent->entries.last;
	entry->next = 0;
	if (parent->entries.last) parent->entries.last->next = entry;
	else parent->entries.first = entry;
	parent->entries.last = entry;
	
	return true;
}

libsqfs_inode_t
libsqfs_directory_inode_downcast(libsqfs_directory_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}


void
libsqfs_directory_table_layout(libsqfs_image_t image, libsqfs_directory_table * dir_table)
{
	dir_table->size = 0;
	libsqfs_directory_inode_t dir;
	for(dir=image->dir_table.dirs.first; dir; dir = dir->next_dir) {
		dir->dir_table_offset = dir_table->size;
		dir_table->size += libsqfs_directory_encoded_size(dir);
	}
}

bool
libsqfs_directory_table_write(libsqfs_image_t image, libsqfs_directory_table * dir_table)
{
	if (!dir_table->size) {
		dir_table->offset = libsqfs_image_reserve(image, 0);
		return true;
	}
	
	void * data = malloc(dir_table->size), * current = data;
	if (!data) return false;
	libsqfs_directory_inode_t dir;
	for(dir=image->dir_table.dirs.first; dir; dir = dir->next_dir) {
		libsqfs_directory_encode(dir, current);
		
		current = libsqfs_directory_encoded_size(dir) + (char *) current;
	}
	
	libsqfs_off_t offset = libsqfs_write_metatable(image, data, dir_table->size, false, false);
	
	dir_table->offset = offset;
	free(data);
	
	return offset != -1;
}

void
libsqfs_directory_table_init(libsqfs_directory_table * dir_table)
{
	dir_table->dirs.first = dir_table->dirs.last = 0;
}

