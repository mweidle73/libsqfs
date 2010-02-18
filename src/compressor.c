#include "compressor.h"

libsqfs_compressor_instance *
libsqfs_compressor_open(const libsqfs_compressor * compr)
{
	return compr->open();
}

ssize_t
libsqfs_compressor_instance_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	return i->vmt->compress(i, dst, dst_size, src, src_size);
}

void
libsqfs_compressor_instance_destroy(libsqfs_compressor_instance * i)
{
	i->vmt->destroy(i);
}

/* null compressor for debugging purposes */

static void
null_compressor_instance_destroy(libsqfs_compressor_instance * i)
{
	free(i);
}

static ssize_t
null_compressor_instance_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	return -1;
}

static const libsqfs_compressor_instance_vmt null_compressor_vmt = {
	.destroy = &null_compressor_instance_destroy,
	.compress = &null_compressor_instance_compress
};

static libsqfs_compressor_instance *
null_compressor_open(void)
{
	libsqfs_compressor_instance * i = malloc(sizeof(*i));
	if (!i) return 0;
	
	i->vmt = &null_compressor_vmt;
	return i;
}

const libsqfs_compressor libsqfs_compressor_null = {
	.open = &null_compressor_open,
	.id = 0
};
