#include "internal.h"

static void
libsqfs_image_options_defaults(libsqfs_image_options_t options)
{
	options->inode_compression = false;
	options->data_compression = false;
	options->fragment_compression = false;
	options->fragments = libsqfs_fragments_tail;
	options->exportable = false;
	options->compression_method = 1;
	options->padding = true;
}

libsqfs_image_options_t
libsqfs_image_options_create(void)
{
	libsqfs_image_options_t options = malloc(sizeof(*options));
	if (!options) return 0;
	libsqfs_image_options_defaults(options);
	return options;
}

void
libsqfs_image_options_destroy(libsqfs_image_options_t options)
{
	free(options);
}

void
libsqfs_image_options_set_inode_compression(libsqfs_image_options_t options, bool compress)
{
	options->inode_compression = compress;
}

void
libsqfs_image_options_set_data_compression(libsqfs_image_options_t options, bool compress)
{
	options->data_compression = compress;
}


void
libsqfs_image_options_set_fragment_compression(libsqfs_image_options_t options, bool compress)
{
	options->fragment_compression = compress;
}

void
libsqfs_image_options_set_exportable(libsqfs_image_options_t options, bool exportable)
{
	options->exportable = exportable;
}

void
libsqfs_image_options_set_padding(libsqfs_image_options_t options, bool padding)
{
	options->padding = padding;
}

void
libsqfs_image_options_set_fragment_option(libsqfs_image_options_t options, libsqfs_fragments_option fragments)
{
	options->fragments = fragments;
}

libsqfs_image_t
libsqfs_image_create(libsqfs_destination_t destination, libsqfs_image_options_t options)
{
	libsqfs_image_t image = malloc(sizeof(*image));
	if (!image) {
		errno = ENOMEM;
		return 0;
	}
	
	if (options) image->options = *options;
	else libsqfs_image_options_defaults(&image->options);
	image->dst = destination;
	image->size = 0;
	image->creation_time = 0;
	image->block_size_log = 17 /* SQUASHFS_FILE_LOG */;
	image->block_size = 1 << image->block_size_log;
	image->compression_method = 1 /* ZLIB_COMPRESSION */;
	image->state = libsqfs_image_building;
	image->dataitems.first = image->dataitems.last = 0;
	image->inodeattrs.first = image->inodeattrs.last = 0;
	image->inodes.first = image->inodes.last = 0;
	image->inodes.count = 0;
	
	image->chunks.submitted.first = image->chunks.submitted.last = 0;
	image->chunks.next_pending = 0;
	image->chunks.nsubmitted = image->chunks.ncompleted = 0;
	image->chunks.done = false;
	pthread_mutex_init(&image->chunks.lock, 0);
	pthread_cond_init(&image->chunks.cond, 0);
	
	image->root = 0;
	
	libsqfs_idtable_init(&image->idtable);
	libsqfs_directory_table_init(&image->dir_table);
	libsqfs_fragment_table_init(&image->frag_table);
	
	libsqfs_reserve_superblock(image);
	
	return image;
}

libsqfs_image_state_t
libsqfs_image_state(libsqfs_image_t image)
{
	return image->state;
}

libsqfs_image_state_t
libsqfs_image_close(libsqfs_image_t image)
{
	libsqfs_image_state_t state = libsqfs_image_finalize(image);
	
	libsqfs_data_t data = image->dataitems.first;
	while(data) {
		libsqfs_data_t next = data->next;
		libsqfs_data_destroy(data);
		data = next;
	}
	
	libsqfs_inode_t inode = image->inodes.first;
	while(inode) {
		libsqfs_inode_t next = inode->next;
		libsqfs_inode_destroy(inode);
		inode = next;
	}
	
	libsqfs_inodeattr_t inodeattr = image->inodeattrs.first;
	while(inodeattr) {
		libsqfs_inodeattr_t next = inodeattr->next;
		libsqfs_inodeattr_destroy(inodeattr);
		inodeattr = next;
	}
	
	libsqfs_fragment_table_destroy(&image->frag_table);
	
	free(image);
	
	return state;
}

libsqfs_image_state_t
libsqfs_image_finalize(libsqfs_image_t image)
{
	if (image->state != libsqfs_image_building)
		return image->state;
	
	bool success = true;
	
	if (!image->root) success = false;
	
	success = success && libsqfs_image_flush_fragments(image);
	libsqfs_finish_chunks(image);
	
	if (success) {
		libsqfs_inode_table_layout(image, &image->inode_table);
		libsqfs_directory_table_layout(image, &image->dir_table);
	}
	
	success = success && libsqfs_inode_table_write(image, &image->inode_table);
	success = success && libsqfs_directory_table_write(image, &image->dir_table);
	success = success && libsqfs_fragment_table_write(image, &image->frag_table);
	success = success && libsqfs_idtable_write(image, &image->idtable);
	success = success && libsqfs_write_superblock(image);
	
	if (success && image->options.padding) {
		libsqfs_off_t padded_size = (image->size + 4095) & ~4095;
		if (padded_size != image->size)
			libsqfs_truncate(image->dst, padded_size);
	}
	
	if (success) image->state = libsqfs_image_finalized;
	else image->state = libsqfs_image_error;
	
	return image->state;
}

bool
libsqfs_image_set_root(libsqfs_image_t image, libsqfs_directory_inode_t root)
{
	if (image->root || image->state != libsqfs_image_building) return false;
	image->root = root;
	root->nlink++;
	return true;
}

libsqfs_directory_inode_t
libsqfs_image_get_root(libsqfs_image_t image)
{
	return image->root;
}

libsqfs_off_t
libsqfs_image_reserve(libsqfs_image_t image, size_t bytes)
{
	libsqfs_off_t current = image->size;
	image->size += bytes;
	return current;
}

