#include "metatable.h"

#include <string.h>

#include "internal.h"

/* FIXME: the signature of the following function is not to my liking; it should
be split into two functions, one writing just the tables, and another adding
the index table -- controlling this with a flag is just ugly

Additionally, the function does no error checking currently */
libsqfs_off_t
libsqfs_write_metatable(libsqfs_image_t image, void * data, size_t size, bool compressed, bool index_tables)
{
	if (!size) return 0;
	
	libsqfs_compressor_instance * ci = libsqfs_compressor_open(image->options.compressor);
	if (!ci) return -1;
	
	size_t ntables = (size + SQUASHFS_METADATA_SIZE-1) / SQUASHFS_METADATA_SIZE, n;
	libsqfs_off_t tables[ntables];
	
	for(n=0; n<ntables; n++) {
		size_t current_table_size = SQUASHFS_METADATA_SIZE;
		if (current_table_size > size) current_table_size = size;
		
		char buffer[current_table_size];
		void * ptr = data;
		size_t storage_size = current_table_size;
		bool compression_successful = false;
		
		if (compressed) {
			ssize_t compressed_size = libsqfs_compressor_instance_compress(ci, buffer, current_table_size, data, current_table_size);
			
			compression_successful = (compressed_size != -1);
			if (compression_successful) {
				ptr = buffer;
				storage_size = compressed_size;
			}
		}
		
		libsqfs_off_t offset = libsqfs_image_reserve(image, storage_size + 2);
		tables[n] = offset;
		
		/* COMPRESSED_BIT actually means "uncompressed"... */
		uint16_t header = current_table_size |
			(compression_successful ? 0: SQUASHFS_COMPRESSED_BIT);
		header = cpu_to_le16(header);
		libsqfs_pwrite(image->dst, &header, sizeof(header), offset);
		libsqfs_pwrite(image->dst, ptr, storage_size, offset+2);
		
		
		data = current_table_size + (char *) data;
		size -= current_table_size;
	}
	
	libsqfs_compressor_instance_destroy(ci);
	
	if (!index_tables) return tables[0];
	
	for(n=0; n<ntables; n++)
		tables[n] = cpu_to_le64(tables[n]);
	
	libsqfs_off_t offset = libsqfs_image_reserve(image, sizeof(tables));
	libsqfs_pwrite(image->dst, &tables, sizeof(tables), offset);
	
	return offset;
}

static void
libsqfs_metablock_destroy(libsqfs_metablock * block)
{
	free(block);
}

void
libsqfs_metatable_init(libsqfs_metatable * tab, libsqfs_compressor_instance * compressor)
{
	tab->first = tab->last = tab->opened_block = 0;
	tab->nmetablocks = 0;
	tab->opened_block_fill = 0;
	tab->compressor = compressor;
	tab->size = 0;
}

void
libsqfs_metatable_destroy(libsqfs_metatable * tab)
{
	if (tab->opened_block)
		libsqfs_metablock_destroy(tab->opened_block);
	libsqfs_metablock * block = tab->first;
	while(block) {
		libsqfs_metablock * next = block->next;
		libsqfs_metablock_destroy(block);
		block = next;
	}
}

static void
libsqfs_metatable_flush(libsqfs_metatable * tab)
{
	if (!tab->opened_block) return;
	
	libsqfs_metablock * block = tab->opened_block;
	block->size = tab->opened_block_fill;
	tab->opened_block = 0;
	tab->opened_block_fill = 0;
	
	char buffer[SQUASHFS_METADATA_SIZE];
	ssize_t compressed_size = -1;
	
	if (tab->compressor)
		compressed_size = libsqfs_compressor_instance_compress(tab->compressor,
			buffer, block->size, block->data, block->size);
	
	if (compressed_size != -1) {
		memcpy(block->data, buffer, compressed_size);
		block->size = compressed_size;
		block->compressed = true;
	}
	
	tab->size += block->size + 2;
	block->prev = tab->last;
	block->next = 0;
	if (tab->last) tab->last->next = block;
	else tab->first = block;
	tab->last = block;
	tab->nmetablocks ++;
}

static libsqfs_metablock *
libsqfs_metatable_getblock(libsqfs_metatable * tab)
{
	if (tab->opened_block) return tab->opened_block;
	
	libsqfs_metablock * block = malloc(sizeof(*block));
	if (!block) return false;
	
	block->prev = block->next = 0;
	block->size = SQUASHFS_METADATA_SIZE;
	block->offset = tab->size;
	memset(block->data, 0, SQUASHFS_METADATA_SIZE);
	block->compressed = false;
	
	tab->opened_block = block;
	tab->opened_block_fill = 0;
	return block;
}

bool
libsqfs_metatable_append(libsqfs_metatable * tab, const void * data, size_t count, 
libsqfs_metatable_entry * pos)
{
	pos->block = tab->size;
	pos->offset = tab->opened_block_fill;
	
	while(count) {
		libsqfs_metablock * block = libsqfs_metatable_getblock(tab);
		
		if (!block) return false;
		size_t to_copy = SQUASHFS_METADATA_SIZE - tab->opened_block_fill;
		if (to_copy > count) to_copy = count;
		
		memcpy(block->data + tab->opened_block_fill, data, to_copy);
		
		tab->opened_block_fill += to_copy;
		count -= to_copy;
		data = to_copy + (char *)data;
		
		if (tab->opened_block_fill == SQUASHFS_METADATA_SIZE)
			libsqfs_metatable_flush(tab);
	}
	
	return true;
}

libsqfs_off_t
libsqfs_metatable_write(libsqfs_metatable * tab, libsqfs_image_t image)
{
	libsqfs_metatable_flush(tab);
	
	libsqfs_off_t start = libsqfs_image_reserve(image, tab->size);
	libsqfs_off_t offset = start;
	
	libsqfs_metablock * block = tab->first;
	while(block) {
		/* COMPRESSED_BIT actually means "uncompressed"... */
		uint16_t header = block->size |
			(block->compressed ? 0: SQUASHFS_COMPRESSED_BIT);
		header = cpu_to_le16(header);
		
		ssize_t written;
		written = libsqfs_pwrite(image->dst, &header, sizeof(header), offset);
		if (written != 2) return -1;
		written = libsqfs_pwrite(image->dst, block->data, block->size, offset+2);
		if (written != block->size) return -1;
		
		offset = offset + block->size + 2;
		block = block->next;
	}
	
	return start;
}
