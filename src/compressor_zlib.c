#include "internal.h"

#include <zlib.h>

typedef struct _libsqfs_compressor_instance_zlib {
	const libsqfs_compressor_instance_vmt * vmt;
	z_stream strm;
} libsqfs_compressor_instance_zlib;

static void
libsqfs_compressor_instance_zlib_destroy(libsqfs_compressor_instance * i)
{
	libsqfs_compressor_instance_zlib * zi = (libsqfs_compressor_instance_zlib *)i;
	deflateEnd(&zi->strm);
	free(zi);
}

static ssize_t
libsqfs_compressor_instance_zlib_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size)
{
	ssize_t written = 0;
	libsqfs_compressor_instance_zlib * zi = (libsqfs_compressor_instance_zlib *)i;
	
	zi->strm.next_in = (void *)src;
	zi->strm.avail_in = src_size;
	zi->strm.next_out = dst;
	zi->strm.avail_out = dst_size;
	
	deflate(&zi->strm, Z_NO_FLUSH);
	int result = deflate(&zi->strm, Z_FINISH);
	if (result != Z_STREAM_END)
		written = -1;
	else
		written = zi->strm.total_out;
	return written;
}

static const libsqfs_compressor_instance_vmt libsqfs_compressor_instance_zlib_vmt = {
	.destroy = &libsqfs_compressor_instance_zlib_destroy,
	.compress = &libsqfs_compressor_instance_zlib_compress
};

static libsqfs_compressor_instance *
libsqfs_compressor_zlib_open(void)
{
	libsqfs_compressor_instance_zlib * zi = malloc(sizeof(*zi));
	if (!zi) return 0;
	
	zi->vmt = &libsqfs_compressor_instance_zlib_vmt;
	zi->strm.zalloc = Z_NULL;
	zi->strm.zfree = Z_NULL;
	if (deflateInit(&zi->strm, Z_DEFAULT_COMPRESSION) != Z_OK) {
		free(zi);
		return 0;
	}
	
	return (libsqfs_compressor_instance *) zi;
}

const libsqfs_compressor libsqfs_compressor_zlib = {
	.open = &libsqfs_compressor_zlib_open,
	.id = 1
};
