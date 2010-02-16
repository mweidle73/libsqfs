#include "internal.h"
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

struct _libsqfs_data_vmt {
	libsqfs_off_t (*get_size)(libsqfs_data_t data);
	ssize_t (*pread)(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset);
	void (*destroy)(libsqfs_data_t data);
};

libsqfs_off_t
libsqfs_data_get_size(libsqfs_data_t data)
{
	return data->vmt->get_size(data);
}

ssize_t
libsqfs_data_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	return data->vmt->pread(data, buffer, size, offset);
}

void
libsqfs_data_destroy(libsqfs_data_t data)
{
	data->vmt->destroy(data);
}

typedef struct _libsqfs_filedata {
	const libsqfs_data_vmt * vmt;
	libsqfs_data_t prev, next;
	libsqfs_image_t image;
	char * pathname;
} libsqfs_filedata;

static libsqfs_off_t
libsqfs_filedata_get_size(libsqfs_data_t data)
{
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	struct stat64 st;
	int error=stat64(filedata->pathname, &st);
	if (error) return -1;
	else return st.st_size;
}

static ssize_t
libsqfs_filedata_pread(libsqfs_data_t data, void * buffer, size_t size, libsqfs_off_t offset)
{
	/* currently, a new file descriptor is opened for every operation;
	this obviuosly sucks, but for now it is good enough */
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	int fd = open(filedata->pathname, O_RDONLY|O_LARGEFILE);
	if (fd<0) return -1;
	
	ssize_t count = pread64(fd, buffer, size, offset);
	close(fd);
	
	return count;
}

static void
libsqfs_filedata_destroy(libsqfs_data_t data)
{
	libsqfs_filedata * filedata = (libsqfs_filedata *)data;
	free(filedata->pathname);
	free(filedata);
}

const libsqfs_data_vmt libsqfs_filedata_vmt = {
	.get_size = &libsqfs_filedata_get_size,
	.pread = &libsqfs_filedata_pread,
	.destroy = &libsqfs_filedata_destroy
};

static void
libsqfs_data_init(libsqfs_image_t image, libsqfs_data_t data)
{
	data->image = image;
	data->prev = image->dataitems.last;
	data->next = 0;
	if (image->dataitems.last) image->dataitems.last->next = data;
	else image->dataitems.first = data;
	image->dataitems.last = data;
}

libsqfs_data_t
libsqfs_data_create_from_file(libsqfs_image_t image, const char * srcpath)
{
	libsqfs_filedata * data = malloc(sizeof(*data));
	if (!data) return 0;
	
	data->pathname = strdup(srcpath);
	if (!data->pathname) {
		/* FIXME: flag error on image */
		free(data);
		return 0;
	}
	
	libsqfs_data_init(image, (libsqfs_data_t)data);
	
	data->vmt = &libsqfs_filedata_vmt;
	
	return (libsqfs_data_t)data;
}

