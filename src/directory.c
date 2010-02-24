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

static void
libsqfs_directory_entry_destroy(libsqfs_directory_entry * entry)
{
	free(entry->name);
	free(entry);
}

/* the following function takes as "reference" the first entry after the
header coded last; it may be NULL if no header has been coded */
static bool
need_new_dir_header(libsqfs_directory_entry * entry, libsqfs_directory_entry * reference)
{
	if (!reference) return true;
	if ((entry->inode->inode_table_entry.block) != (reference->inode->inode_table_entry.block))
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
			
			hdr->start_block = cpu_to_le32(entry->inode->inode_table_entry.block);
			hdr->inode_number = cpu_to_le32(entry->inode->inode_number);
		}
		
		struct squashfs_dir_entry * ent = dst;
		dst = ent + 1;
		ent->offset = cpu_to_le16(entry->inode->inode_table_entry.offset);
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
	
	libsqfs_directory_entry * entry = dir->entries.first;
	while(entry) {
		libsqfs_directory_entry * next = entry->next;
		libsqfs_directory_entry_destroy(entry);
		entry = next;
	}
	free(dir);
}

static bool
libsqfs_directory_inode_encode(libsqfs_directory_inode_t dir, libsqfs_metatable * tab, libsqfs_metatable_entry * pos)
{
	struct squashfs_dir_inode_header hdr;
	
	/* FIXME: add support for encoding SQUASHFS_LDIR_TYPE */
	dir->encoded_type = SQUASHFS_DIR_TYPE;
	
	hdr.inode_type = cpu_to_le16(dir->encoded_type);
	hdr.mode = cpu_to_le16(dir->attr->mode);
	hdr.uid = cpu_to_le16(dir->attr->mapped_uid);
	hdr.guid = cpu_to_le16(dir->attr->mapped_gid);
	hdr.mtime = cpu_to_le32(dir->attr->ctime);
	hdr.inode_number = cpu_to_le32(dir->inode_number);
	
	hdr.start_block = cpu_to_le32(dir->dir_table_entry.block);
	hdr.offset = cpu_to_le16(dir->dir_table_entry.offset);
	
	hdr.nlink = cpu_to_le32(dir->nlink);
	
	hdr.file_size = cpu_to_le16(libsqfs_directory_encoded_size(dir) + 3);
	
	if (dir->parent)
		hdr.parent_inode = cpu_to_le32(dir->parent->inode_number);
	else
		hdr.parent_inode = cpu_to_le32(dir->image->inodes.count+1);
	
	return libsqfs_metatable_append(tab, &hdr, sizeof(hdr), pos);
}

static bool
libsqfs_directory_serialize(libsqfs_inode_t inode)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	libsqfs_image_t image = dir->image;
	
	/* first make sure all directory entries are serialized */
	libsqfs_directory_entry * entry = dir->entries.first;
	while(entry) {
		if (!libsqfs_inode_serialize(entry->inode)) return false;
		entry = entry->next;
	}
	
	/* first, encode directory entries and write them to the
	directory table */
	size_t entry_list_size = libsqfs_directory_encoded_size(dir);
	char entry_list[entry_list_size];
	libsqfs_directory_encode(dir, entry_list);
	if (!libsqfs_metatable_append(&image->dir_table.tab, entry_list,
		entry_list_size, &dir->dir_table_entry)) return false;
	
	/* now encode the inode itself, referencing the directory table */
	if (!libsqfs_directory_inode_encode(dir, &image->inode_table.tab, &dir->inode_table_entry))
		return false;
	
	return true;
}

const libsqfs_inode_vmt libsqfs_directory_inode_vmt = {
	.destroy = &libsqfs_directory_inode_destroy,
	.serialize = &libsqfs_directory_serialize,
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
	dir->parent = 0;
	libsqfs_inode_init(image, (libsqfs_inode_t) dir);
	
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
	
	/* file names must be kept sorted in directory listings; determine
	position where to insert */
	libsqfs_directory_entry * insert_before = parent->entries.first, * insert_after = 0;
	while (insert_before && strcmp(name, insert_before->name)>0) {
		insert_after = insert_before;
		insert_before = insert_before->next;
	}
	
	entry->prev = insert_after;
	entry->next = insert_before;
	
	if (insert_after) insert_after->next = entry;
	else parent->entries.first = entry;
	if (insert_before) insert_before->prev = entry;
	else parent->entries.last = entry;
	
	return true;
}

libsqfs_inode_t
libsqfs_directory_inode_downcast(libsqfs_directory_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}

void
libsqfs_directory_table_init(libsqfs_directory_table * dir_table, libsqfs_compressor_instance * compressor)
{
	libsqfs_metatable_init(&dir_table->tab, compressor);
	dir_table->offset = -1;
}

bool
libsqfs_directory_table_write(libsqfs_image_t image, libsqfs_directory_table * dir_table)
{
	libsqfs_off_t offset = libsqfs_metatable_write(&dir_table->tab, image);
	if (offset == -1) return false;
	dir_table->offset = offset;
	return true;
}

void
libsqfs_directory_table_destroy(libsqfs_directory_table * dir_table)
{
	libsqfs_metatable_destroy(&dir_table->tab);
}

