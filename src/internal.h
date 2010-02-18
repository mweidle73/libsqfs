#ifndef LIBSQFS_INTERNAL_H
#define LIBSQFS_INTERNAL_H

#include <libsqfs.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>

/* destinations */

typedef long long libsqfs_off_t;

ssize_t
libsqfs_pwrite(libsqfs_destination_t destination, const void * buffer, size_t size, libsqfs_off_t offset);

void
libsqfs_truncate(libsqfs_destination_t destination, libsqfs_off_t offset);

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

/* compressor */

typedef struct _libsqfs_compressor libsqfs_compressor;

extern const libsqfs_compressor libsqfs_compressor_zlib;
extern const libsqfs_compressor libsqfs_compressor_null;

typedef struct _libsqfs_compressor_instance libsqfs_compressor_instance;

struct _libsqfs_compressor {
	libsqfs_compressor_instance * (*open)(void);
	int id;
};

typedef struct _libsqfs_compressor_instance_vmt {
	void (*destroy)(libsqfs_compressor_instance * i);
	ssize_t (*compress)(libsqfs_compressor_instance * i,
		void * dst, size_t dst_size, const void * src, size_t src_size);
} libsqfs_compressor_instance_vmt;

struct _libsqfs_compressor_instance {
	const libsqfs_compressor_instance_vmt * vmt;
};

libsqfs_compressor_instance *
libsqfs_compressor_open(const libsqfs_compressor * compr);

/* transforms the given input data and returns the number of bytes of
the output area used; if the block could not be compressed (e.g. because
the compressed representation turns out to be larger than the provided
buffer), (size_t)-1 is returned */
ssize_t
libsqfs_compressor_instance_compress(libsqfs_compressor_instance * i,
	void * dst, size_t dst_size, const void * src, size_t src_size);

void
libsqfs_compressor_instance_destroy(libsqfs_compressor_instance * i);

/* chunks, representing bulk data (either as complete blocks or fragments)
to be written to the image */

typedef enum {
	libsqfs_chunk_pending,
	libsqfs_chunk_compressed,
	libsqfs_chunk_finished
} libsqfs_chunk_state_t;

typedef struct _libsqfs_chunk libsqfs_chunk;
struct _libsqfs_chunk {
	libsqfs_chunk * prev, * next;
	libsqfs_image_t image;
	libsqfs_chunk_state_t state;
	
	bool may_compress;
	
	libsqfs_data_piece src;
	struct {
		libsqfs_off_t offset;
		size_t size;
		void * data;
		bool compressed;
	} dst;
};
typedef struct _libsqfs_chunk_list {
	libsqfs_chunk * first, * last;
} libsqfs_chunk_list;

void
libsqfs_image_submit_chunk(libsqfs_image_t image, libsqfs_chunk * chunk);

libsqfs_chunk *
libsqfs_image_submit_chunk_for_data(libsqfs_image_t image, libsqfs_data_t data,
	libsqfs_off_t offset, size_t size, bool may_compress);

void
libsqfs_finish_chunks(libsqfs_image_t image);

void
libsqfs_chunk_destroy(libsqfs_chunk * chunk);

/* inodes */

struct _libsqfs_inodeattr {
	libsqfs_inodeattr_t prev, next;
	uid_t uid;
	gid_t gid;
	mode_t mode;
	time_t ctime;
	
	uint16_t mapped_uid, mapped_gid;
};

void
libsqfs_inodeattr_destroy(libsqfs_inodeattr_t attr);

typedef struct _libsqfs_inode_vmt libsqfs_inode_vmt;

struct _libsqfs_inode_vmt {
	void (*destroy)(libsqfs_inode_t inode);
	size_t (*encoded_size)(libsqfs_inode_t inode);
	void (*encode)(libsqfs_inode_t inode, void * dst);
};

#define LIBSQFS_INODE_COMMON \
	const libsqfs_inode_vmt * vmt; \
	libsqfs_inode_t prev, next; \
	libsqfs_image_t image; \
	\
	libsqfs_inodeattr_t attr; \
	size_t nlink; \
	\
	size_t inode_number; \
	long long squashfs_inode; /* this is the on-disk "inode", consisting of encoded block+offset */\
	int encoded_type; \

struct _libsqfs_inode {
	LIBSQFS_INODE_COMMON
};

void
libsqfs_inode_init(libsqfs_image_t image, libsqfs_inode_t inode);

void
libsqfs_inode_destroy(libsqfs_inode_t inode);

typedef struct _libsqfs_directory_entry libsqfs_directory_entry;

struct _libsqfs_directory_inode {
	LIBSQFS_INODE_COMMON
	
	libsqfs_directory_inode_t parent;
	struct {
		libsqfs_directory_entry * first, * last;
	} entries;
	
	libsqfs_directory_inode_t prev_dir, next_dir;
	
	libsqfs_off_t dir_table_offset;
};

typedef struct _libsqfs_inode_table {
	libsqfs_off_t offset;
	libsqfs_off_t size;
} libsqfs_inode_table;

void
libsqfs_inode_table_layout(libsqfs_image_t image, libsqfs_inode_table * inode_table);

bool
libsqfs_inode_table_write(libsqfs_image_t image, libsqfs_inode_table * inode_table);

/* directories */

typedef struct _libsqfs_directory_table {
	libsqfs_off_t offset;
	libsqfs_off_t size;
	
	struct {
		libsqfs_directory_inode_t first, last;
	} dirs;
} libsqfs_directory_table;

void
libsqfs_directory_table_init(libsqfs_directory_table * dir_table);

void
libsqfs_directory_table_layout(libsqfs_image_t image, libsqfs_directory_table * dir_table);

bool
libsqfs_directory_table_write(libsqfs_image_t image, libsqfs_directory_table * dir_table);

/* fragments */

typedef struct _libsqfs_fragment_piece libsqfs_fragment_piece;
typedef struct _libsqfs_fragment_block libsqfs_fragment_block;

/* tails of multiple files may be grouped into a single fragment block;
a "piece" describes this composition of a fragment block of multiple
parts, and allows files to identify "their" tail piece */
struct _libsqfs_fragment_piece {
	libsqfs_fragment_piece * prev, * next;
	libsqfs_fragment_block * fragment;
	/* placement within fragment block */
	size_t offset;
	
	/* description of where source data originates from */
	libsqfs_data_piece src;
};

struct _libsqfs_fragment_block {
	libsqfs_fragment_block * prev, * next;
	size_t index, size;
	libsqfs_chunk * chunk;
	
	struct {
		libsqfs_fragment_piece * first, * last;
		size_t count;
	} pieces;
};

typedef struct _libsqfs_fragment_table {
	libsqfs_off_t offset;
	struct {
		libsqfs_fragment_block * first, * last;
		size_t count;
	} fragments;
	
	libsqfs_fragment_block * open_fragment;
} libsqfs_fragment_table;

libsqfs_fragment_piece *
libsqfs_image_submit_fragment_piece(libsqfs_image_t image, libsqfs_data_t data, size_t size, libsqfs_off_t offset);

bool
libsqfs_image_flush_fragments(libsqfs_image_t image);

void
libsqfs_fragment_table_init(libsqfs_fragment_table * frag_table);

void
libsqfs_fragment_table_destroy(libsqfs_fragment_table * frag_table);

bool
libsqfs_fragment_table_write(libsqfs_image_t image, libsqfs_fragment_table * frag_table);

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
	
	libsqfs_directory_inode_t root;
};

/* reserve space in image */
libsqfs_off_t
libsqfs_image_reserve(libsqfs_image_t image, size_t bytes);

bool
libsqfs_write_superblock(libsqfs_image_t image);

void
libsqfs_reserve_superblock(libsqfs_image_t image);

libsqfs_off_t
libsqfs_write_metatable(libsqfs_image_t image, void * data, size_t size, bool compressed, bool index_tables);

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
