#include "internal.h"

bool
libsqfs_fragment_table_write(libsqfs_image_t image, libsqfs_fragment_table * frag_table)
{
	frag_table->size = 0;
	if (!frag_table->size) {
		frag_table->offset = libsqfs_image_reserve(image, 0);
		return true;
	}
	return true;
}

