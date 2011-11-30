/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2010
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
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

#include "internal.h"

#include <string.h>
#include <limits.h>

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
	
	The "squashfs_dir_header" is put into memory together with the first
	entry following it -- no separate data structure will be allocated
	for it.
	
	Directories can (optionally) be indexed: This is a "thinned-out" list
	of entries with pointers into the middle of the list. It helps to
	avoid traversing the full list by directly jumping to a location
	"close" to the probable target. Only entries which have an encoded
	"squashfs_dir_header" immediately preceding it can be indexed.
*/

struct libsqfs_directory_entry {
	/* linked list of all entries */
	libsqfs_directory_entry * prev, * next;
	/* list to next "indexed" entry */
	libsqfs_directory_entry * next_indexed;
	/* name as it appears in the directory listing */
	char * name;
	/* file this entry points to */
	libsqfs_inode_t inode;
	
	/* serial number of entry */
	size_t serialno;
	/* offset from start of directory table */
	size_t offset;
	/* location in directory table */
	libsqfs_metatable_entry directory_table_entry;
	
	/* on-disk representation of this entry; will be filled during write-out */
	void * encoded_data;
	/* number of bytes in on-disk representation */
	size_t encoded_size;
};

static void
libsqfs_directory_entry_destroy(libsqfs_directory_entry * entry)
{
	free(entry->name);
	if (entry->encoded_data) free(entry->encoded_data);
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
	
	if (entry->encoded_size + entry->offset - reference->offset > SQUASHFS_METADATA_SIZE)
		return true;
	
	if (entry->serialno - reference->serialno > 255)
		return true;
	
	return false;
}

static bool
libsqfs_directory_encode(libsqfs_directory_inode_t dir, libsqfs_metatable * tab)
{
	/* this is the count of entries after the squashfs_dir_header coded last;
	the total number of elements following the header must be coded into the
	header itself, so track this count until reaching the point where a new
	header must be inserted and revisit the previous one */
	uint32_t count = 0;
	libsqfs_directory_entry * entry = dir->entries.first, * reference = 0;
	libsqfs_directory_entry ** next_indexed = &dir->first_indexed;
	
	size_t offset = 0, serialno = 0;
	
	while(entry) {
		size_t namelen = strlen(entry->name);
		entry->serialno = serialno++;
		entry->offset = offset;
		
		/* preliminary "size estimate", allowing "need_new_dir_header" to
		make a decision */
		entry->encoded_size = sizeof(struct squashfs_dir_entry) + namelen;
		bool insert_dir_header = need_new_dir_header(entry, reference);
		
		if (insert_dir_header)
			entry->encoded_size += sizeof(struct squashfs_dir_header);
		entry->encoded_data = malloc(entry->encoded_size);
		if (!entry->encoded_data) return false;
		
		/* for the following, we rely on the assumption that malloc returns
		"properly" aligned memory even for odd allocation sizes; this *might*
		be false, but I don't know of a platform where it is */
		
		void * p = entry->encoded_data;
		
		if (insert_dir_header) {
			/* revisit "previous" dir header and record entry count */
			if (reference) {
				 ((struct squashfs_dir_header *) reference->encoded_data)->count = cpu_to_le32(count-1);
			}
			
			struct squashfs_dir_header * hdr = p;
			p = hdr +1;
			
			count = 0;
			reference = entry;
			hdr->start_block = cpu_to_le32(entry->inode->inode_table_entry.block);
			hdr->inode_number = cpu_to_le32(entry->inode->inode_number);
			*next_indexed = entry;
			next_indexed = &entry->next_indexed;
			dir->indexed_count ++;
		}
		
		struct squashfs_dir_entry * ent = p;
		p = ent + 1;
		ent->offset = cpu_to_le16(entry->inode->inode_table_entry.offset);
		ent->inode_number = cpu_to_le16(entry->inode->inode_number - reference->inode->inode_number);
		ent->type = cpu_to_le16(entry->inode->encoded_base_type);
		ent->size = cpu_to_le16(namelen - 1);
		
		memcpy(p, entry->name, namelen);
		p = namelen + (char *)p;
		
		count++;
		offset += entry->encoded_size;
		
		entry = entry->next;
	}
	if (reference) {
		 ((struct squashfs_dir_header *) reference->encoded_data)->count = cpu_to_le32(count-1);
	}
	
	dir->encoded_entries_size = offset;
	
	libsqfs_metatable_entry * pos = &dir->dir_table_entry;
	/* write explicit zero block/offset in case there is no
	directory entry at all (kernel squashfs driver does not
	care, but "unsquashfs" is quite unhappy if there are
	garbage values contained here for empty directories) */
	pos->block = 0;
	pos->offset = 0;
	/* write out everything; the memory allocated for holding the encoded
	representations can safely be discarded now */
	entry = dir->entries.first;
	while (entry) {
		if (!libsqfs_metatable_append(tab, entry->encoded_data, entry->encoded_size, &entry->directory_table_entry))
			return false;
		if (pos) *pos = entry->directory_table_entry;
		free(entry->encoded_data);
		entry->encoded_data = 0;
		entry = entry->next;
		pos = 0;
	}
	
	return true;
}

static void
libsqfs_directory_inode_prune_index(libsqfs_directory_inode_t dir)
{
	/* make sure that indexed entries are spaced apart by
	at least 128 entries; this prevents creating "too many"
	index entries */
	libsqfs_directory_entry * current = dir->first_indexed;
	while(current && current->next_indexed) {
		libsqfs_directory_entry * next_indexed = current->next_indexed;
		if (next_indexed->serialno - current->serialno > 128) {
			current = next_indexed;
		} else  {
			current->next_indexed = next_indexed->next_indexed;
			dir->indexed_count --;
		}
	}
	
	/* there is no point in including the very first entry in
	the index; always skip it */
	if (dir->first_indexed) {
		dir->first_indexed = dir->first_indexed->next_indexed;
		dir->indexed_count --;
	}
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
libsqfs_directory_inode_encode(libsqfs_directory_inode_t dir, libsqfs_metatable * tab)
{
	if (dir->base.encoded_type == SQUASHFS_DIR_TYPE) {
		struct squashfs_dir_inode_header hdr;
		
		hdr.inode_type = cpu_to_le16(dir->base.encoded_type);
		hdr.mode = cpu_to_le16(dir->base.attr->mode);
		hdr.uid = cpu_to_le16(dir->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(dir->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(dir->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(dir->base.inode_number);
		
		hdr.start_block = cpu_to_le32(dir->dir_table_entry.block);
		hdr.offset = cpu_to_le16(dir->dir_table_entry.offset);
		
		hdr.nlink = cpu_to_le32(dir->base.nlink);
		
		hdr.file_size = cpu_to_le16(dir->encoded_entries_size + 3);
		
		if (dir->parent)
			hdr.parent_inode = cpu_to_le32(dir->parent->base.inode_number);
		else
			hdr.parent_inode = cpu_to_le32(dir->base.image->inodes.count+1);
		
		return libsqfs_metatable_append(tab, &hdr, sizeof(hdr), &dir->base.inode_table_entry);
	} else {
		struct squashfs_ldir_inode_header hdr;
		
		hdr.inode_type = cpu_to_le16(dir->base.encoded_type);
		hdr.mode = cpu_to_le16(dir->base.attr->mode);
		hdr.uid = cpu_to_le16(dir->base.attr->mapped_uid);
		hdr.guid = cpu_to_le16(dir->base.attr->mapped_gid);
		hdr.mtime = cpu_to_le32(dir->base.attr->ctime);
		hdr.inode_number = cpu_to_le32(dir->base.inode_number);
		
		hdr.nlink = cpu_to_le32(dir->base.nlink);
		hdr.file_size = cpu_to_le32(dir->encoded_entries_size + 3);
		hdr.start_block = cpu_to_le32(dir->dir_table_entry.block);
		
		if (dir->parent)
			hdr.parent_inode = cpu_to_le32(dir->parent->base.inode_number);
		else
			hdr.parent_inode = cpu_to_le32(dir->base.image->inodes.count+1);
		
		hdr.i_count = cpu_to_le16(dir->indexed_count);
		hdr.offset = cpu_to_le16(dir->dir_table_entry.offset);
		
		if (dir->base.attr->xattrset)
			hdr.xattr = cpu_to_le32(dir->base.attr->xattrset->id);
		else
			hdr.xattr = cpu_to_le32(-1);
		
		if (!libsqfs_metatable_append(tab, &hdr, sizeof(hdr), &dir->base.inode_table_entry))
			return false;
		
		libsqfs_directory_entry * entry = dir->first_indexed;
		while(entry) {
			size_t namelen = strlen(entry->name);
			struct squashfs_dir_index index;
			index.index = cpu_to_le32(entry->offset);
			index.start_block = cpu_to_le32(entry->directory_table_entry.block);
			index.size = cpu_to_le32(namelen-1);
			if (!libsqfs_metatable_append(tab, &index, sizeof(index), 0))
				return false;
			if (!libsqfs_metatable_append(tab, entry->name, namelen, 0))
				return false;
			
			entry = entry->next_indexed;
		}
		
		return true;
	}
}

static bool
libsqfs_directory_serialize(libsqfs_inode_t inode)
{
	libsqfs_directory_inode_t dir = (libsqfs_directory_inode_t) inode;
	libsqfs_image_t image = dir->base.image;
	
	/* first, make sure inodes of all entries are serialized */
	libsqfs_directory_entry * entry = dir->entries.first;
	while(entry) {
		if (!libsqfs_inode_serialize(entry->inode)) return false;
		entry = entry->next;
	}
	
	/* then encode directory entries and write them to the
	directory table */
	if (!libsqfs_directory_encode(dir, &image->dir_table))
		return false;
	
	/* after encoding, the "index" contains all entries that are "indexable"
	which generally is way too much; thin out the index a bit */
	libsqfs_directory_inode_prune_index(dir);
	
	/* choose encoding type; try to stick with the "smallest" type,
	picking the extended version only if one of the features of this
	directory requires it */
	dir->base.encoded_type = SQUASHFS_DIR_TYPE;
	dir->base.encoded_base_type = SQUASHFS_DIR_TYPE;
	if (dir->indexed_count || dir->encoded_entries_size > USHRT_MAX-3)
		dir->base.encoded_type = SQUASHFS_LDIR_TYPE;
	
	if (dir->base.attr->xattrset)
		dir->base.encoded_type = SQUASHFS_LDIR_TYPE;
	/* now encode the directory inode itself, referencing the previously
	encoded data in the directory table */
	if (!libsqfs_directory_inode_encode(dir, &image->inode_table))
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
	
	dir->base.vmt = &libsqfs_directory_inode_vmt;
	libsqfs_inode_init(image, &dir->base, attr);
	dir->entries.first = dir->entries.last = 0;
	dir->parent = 0;
	dir->indexed_count = 0;
	dir->first_indexed = 0;
	
	return dir;
}

bool
libsqfs_directory_add_entry(libsqfs_directory_inode_t parent, const char * name, libsqfs_inode_t inode)
{
	if (inode->vmt == &libsqfs_directory_inode_vmt) {
		if (((libsqfs_directory_inode_t) inode)->parent) {
			/* this directory already has a parent, it cannot
			have another one; bail out */
			libsqfs_image_flag_error(parent->base.image, "Directory can have only one parent directory", false);
			return false;
		}
		((libsqfs_directory_inode_t) inode)->parent = parent;
		parent->base.nlink++;
	}
	
	libsqfs_directory_entry * entry = malloc(sizeof(*entry));
	if (!entry) {
		libsqfs_image_out_of_memory(parent->base.image, false);
		return false;
	}
	
	entry->name = strdup(name);
	if (!entry->name) {
		free(entry);
		libsqfs_image_out_of_memory(parent->base.image, false);
		return false;
	}
	entry->encoded_size = 0;
	entry->encoded_data = 0;
	entry->next_indexed = 0;
	
	inode->nlink++;
	entry->inode = inode;
	
	/* file names must be kept sorted in directory listings; determine
	position where to insert (this is not the smartest sorting algorithm
	ever invented, I know...) */
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
libsqfs_directory_inode_lookup(libsqfs_directory_inode_t dir, const char * name)
{
	libsqfs_directory_entry * entry = dir->entries.first;
	while(entry && (strcmp(entry->name, name) != 0))
		entry = entry->next;
	
	if (entry) return entry->inode;
	return 0;
}

libsqfs_inode_t
libsqfs_directory_inode_downcast(libsqfs_directory_inode_t inode)
{
	return (libsqfs_inode_t) inode;
}
