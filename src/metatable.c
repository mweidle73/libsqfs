#include "internal.h"
#include "squashfs_fs.h"

libsqfs_off_t
libsqfs_write_metatable(libsqfs_image_t image, void * data, size_t size, bool compressed, bool index_tables)
{
	if (!size) return libsqfs_image_reserve(image, 0);
	size_t ntables = (size + SQUASHFS_METADATA_SIZE-1) / SQUASHFS_METADATA_SIZE, n;
	libsqfs_off_t tables[ntables];
	
	for(n=0; n<ntables; n++) {
		size_t current_table_size = SQUASHFS_METADATA_SIZE;
		if (current_table_size > size) current_table_size = size;
		
		libsqfs_off_t offset = libsqfs_image_reserve(image, current_table_size+2);
		tables[n] = offset;
		
		/* COMPRESSED_BIT actually means "uncompressed"... */
		uint16_t header = current_table_size |
			(compressed ? 0: SQUASHFS_COMPRESSED_BIT);
		header = cpu_to_le16(header);
		libsqfs_pwrite(image->dst, &header, sizeof(header), offset);
		libsqfs_pwrite(image->dst, data, current_table_size, offset+2);
		
		
		data = current_table_size + (char *) data;
		size -= current_table_size;
	}
	
	if (!index_tables) return tables[0];
	
	for(n=0; n<ntables; n++)
		tables[n] = cpu_to_le64(tables[n]);
	
	libsqfs_off_t offset = libsqfs_image_reserve(image, sizeof(tables));
	libsqfs_pwrite(image->dst, &tables, sizeof(tables), offset);
	
	return offset;
}
