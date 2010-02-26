#ifndef LIBSQFS_DATASOURCE_H
#define LIBSQFS_DATASOURCE_H

#include <libsqfs.h>

/* data source items */

typedef struct _libsqfs_data_vmt libsqfs_data_vmt;
struct _libsqfs_data {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
};

/* query size */
libsqfs_off_t
libsqfs_data_get_size(libsqfs_data_t data);

/* raw read */
ssize_t
libsqfs_data_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset);

void
libsqfs_data_destroy(libsqfs_data_t data);


#endif
