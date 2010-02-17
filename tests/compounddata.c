#include <libsqfs.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

ssize_t
libsqfs_data_pread(libsqfs_data_t data, void * buffer, size_t size, long long offset);

void * memdup(const void * src, size_t size)
{
	void * buffer = malloc(size);
	memcpy(buffer, src, size);
	return buffer;
}

libsqfs_data_t
data_block(libsqfs_image_t image, const void * src, size_t size)
{
	void * buffer = memdup(src, size);
	return libsqfs_data_create_for_transferred_buffer(image, buffer, size);
}

int main()
{
	/* This data "block" is shredded into various pieces, that are further
	sliced and concatenated in order to produce another data block that
	should be equivalent to this one. Exercises data block slicing and
	concatenation in libsqfs. */
	char data[]="0123456789";
	
	libsqfs_destination_t dest = libsqfs_destination_create_null();
	libsqfs_image_t image = libsqfs_image_create(dest, 0);
	
	libsqfs_data_piece pieces[] = {
		{data_block(image, &data[0], 10), 2, 0},
		{data_block(image, &data[2], 2), 2, 0},
		{data_block(image, &data[0], 6), 2, 4},
		{data_block(image, &data[6], 4), 2, 0},
		{data_block(image, &data[6], 4), 2, 2}
	};
	
	libsqfs_data_t compound = libsqfs_data_create_compound(image, 5, pieces);
	
	size_t low, high;
	for(low = 0; low<10; low ++) for(high=low; high<10; high++) {
		char tmp[10];
		
		ssize_t bytes = libsqfs_data_pread(compound, tmp, high-low, low);
		assert(bytes == high-low);
		assert(memcmp(tmp, data+low, high-low) == 0);
	}
	
	libsqfs_image_close(image);
	libsqfs_destination_release(dest);
	
	return 0;
}
