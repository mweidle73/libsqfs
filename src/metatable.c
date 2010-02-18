#include "internal.h"
#include "squashfs_fs.h"

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
