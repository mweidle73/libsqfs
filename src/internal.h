#ifndef LIBSQFS_INTERNAL_H
#define LIBSQFS_INTERNAL_H

#include <libsqfs.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

#include "squashfs_fs.h"

#include "compressor.h"
#include "metatable.h"
#include "inodes.h"
#include "chunks.h"
#include "fragment.h"
#include "datasource.h"
#include "threadpool.h"

/* destinations */

ssize_t
libsqfs_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset);

void
libsqfs_truncate(libsqfs_destination_t destination, libsqfs_off_t offset);

/* entry function for worker threads */
void
libsqfs_image_process_chunks(libsqfs_image_t image);

/* id table  */

typedef struct _libsqfs_idtable libsqfs_idtable;
struct _libsqfs_idtable {
	uint32_t * ids;
	size_t nids;
	libsqfs_off_t offset;
};

void
libsqfs_idtable_init(libsqfs_idtable * idtable);

bool
libsqfs_idtable_write(libsqfs_image_t image, libsqfs_idtable * idtable);

void
libsqfs_idtable_destroy(libsqfs_idtable * idtable);

/* returns 16-bit mapped id, or -1 on mapping failure */
int
libsqfs_idtable_map(libsqfs_idtable * idtable, uint32_t id);

/* export table */

typedef struct _libsqfs_export_table {
	libsqfs_off_t offset;
} libsqfs_export_table;

void
libsqfs_export_table_init(libsqfs_export_table * export_tab);

bool
libsqfs_export_table_write(libsqfs_image_t image, libsqfs_export_table * export_tab);

/* images */

typedef struct _libsqfs_image_options libsqfs_image_options;
struct _libsqfs_image_options {
	bool inode_compression;
	bool data_compression;
	bool fragment_compression;
	libsqfs_fragments_option fragments;
	bool exportable;
	const libsqfs_compressor * compressor;
	bool padding;
	size_t block_size, block_size_log;
};

struct _libsqfs_image {
	libsqfs_image_options options;
	libsqfs_destination_t dst;
	libsqfs_off_t size;
	libsqfs_image_state_t state;
	
	uint32_t creation_time;
	
	struct {
		libsqfs_data_t first, last;
	} dataitems;
	struct {
		libsqfs_inodeattr_t first, last;
	} inodeattrs;
	struct {
		libsqfs_inode_t first, last;
		size_t count;
	} inodes;
	
	struct {
		libsqfs_chunk_list submitted;
		libsqfs_chunk * next_pending;
		size_t nsubmitted, ncompleted;
		
		bool done;
		
		pthread_mutex_t lock;
		pthread_cond_t cond;
	} chunks;
	
	libsqfs_idtable idtable;
	libsqfs_inode_table inode_table;
	libsqfs_directory_table dir_table;
	libsqfs_fragment_table frag_table;
	libsqfs_export_table export_table;
	
	libsqfs_compressor_instance * compressor;
	
	libsqfs_directory_inode_t root;
	
	libsqfs_worker_thread * thread_pool;
};

/* reserve space in image */
libsqfs_off_t
libsqfs_image_reserve(libsqfs_image_t image, size_t bytes);

bool
libsqfs_write_superblock(libsqfs_image_t image);

void
libsqfs_reserve_superblock(libsqfs_image_t image);

#include <endian.h>
#if __BYTE_ORDER == __LITTLE_ENDIAN
static inline uint16_t cpu_to_le16(uint16_t v) {return v;}
static inline uint32_t cpu_to_le32(uint32_t v) {return v;}
static inline uint64_t cpu_to_le64(uint64_t v) {return v;}
#elif __BYTE_ORDER == __BIG_ENDIAN
#include <byteswap.h>
static inline uint16_t cpu_to_le16(uint16_t v) {return bswap_16(v);}
static inline uint32_t cpu_to_le32(uint32_t v) {return bswap_32(v);}
static inline uint64_t cpu_to_le64(uint64_t v) {return bswap_64(v);}
#else
#error Unknown endian
#endif

#endif
