/*
 * Library for creating squashfs filesystem images.
 *
 * Copyright (c) 2010
 * secunet Security Networks AG, Helge Bahmann <helge.bahmann@secunet.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Refer to the file "COPYING" for details.
 */

#ifndef LIBSQFS_H
#define LIBSQFS_H

#include <stdbool.h>
#include <sys/types.h>

#ifdef __cpluscplus
extern "C"
{
#endif

/** \brief Type for representing file offsets */
typedef long long libsqfs_off_t;

/**
	\defgroup output_handler Output handler
	
	Handler for writing data to intended output target. This abstraction
	allows for creation of images in files, in memory or other destinations.
*/
/*@{*/

/**
	\brief @c squashfs image output target
*/
typedef struct _libsqfs_destination * libsqfs_destination_t;

/**
	\brief Release destination handle
	\param destination Destination handle
	
	Release this destination handle, closing the handle on the last "release".
*/
void
libsqfs_destination_release(libsqfs_destination_t destination);

/**
	\brief Create output
	\param fd Descriptor of opened file which should be written to
	\return @c squashfs destination handle, or NULL on error with errno set appropriately
	
	The file will be truncated to zero.
*/
libsqfs_destination_t
libsqfs_destination_create_for_filedes(int fd);

/**
	\brief Create output
	\param name Name of file which should be written to
	\param mode Mode of newly-created file
	\return @c squashfs destination handle, or NULL on error with errno set appropriately
	
	The file will be truncated to zero.
*/
libsqfs_destination_t
libsqfs_destination_create_for_file(const char * name, mode_t mode);

/**
	\brief Create NULL output
	\return @c squashfs destination handle, or NULL on error with errno set appropriately
	
	Creates an output handle that simply discards all data (useful for testing).
*/
libsqfs_destination_t
libsqfs_destination_create_null(void);

/*@}*/

/**
	\defgroup images Squashfs images
*/
/*@{*/

/**
	\defgroup image_options Squashfs image options
	
	Options controlling the on-disk layout of the generated filesystem image
*/
/*@{*/

/** \brief Options controlling creation of fragment blocks */
typedef enum {
	/** \brief Never create fragments, store files in blocks */
	libsqfs_fragments_never = 0,
	/** \brief Create fragments only for small files (i.e. smaller than block size) */
	libsqfs_fragments_small = 1,
	/** \brief Create fragments for all files, packing tails of small and large files together */
	libsqfs_fragments_always = 2
} libsqfs_fragments_option;

/** \brief Options influencing on-disk layout of image */
typedef struct _libsqfs_image_options * libsqfs_image_options_t;

/**
	\brief Create handle for image options
	\returns Handle for default image options
	
	Options are set up with library defaults. The structure
	must be destroyed with \ref libsqfs_image_options_destroy
	after the caller is finished using it.
*/
libsqfs_image_options_t
libsqfs_image_options_create(void);

/**
	\brief Destroy image options
	\param options Handle for options to destroy
*/
void
libsqfs_image_options_destroy(libsqfs_image_options_t options);

/**
	\brief Control compression of inode tables
	\param options Handle for options
	\param compress Turn compression on/off
*/
void
libsqfs_image_options_set_inode_compression(libsqfs_image_options_t options, bool compress);

/**
	\brief Control compression of data blocks
	\param options Handle for options
	\param compress Turn compression on/off
*/
void
libsqfs_image_options_set_data_compression(libsqfs_image_options_t options, bool compress);

/**
	\brief Control compression of fragment blocks
	\param options Handle for options
	\param compress Turn compression on/off
*/
void
libsqfs_image_options_set_fragment_compression(libsqfs_image_options_t options, bool compress);

/**
	\brief Control generation of export table (for NFS)
	\param options Handle for options
	\param exportable Control whether file system is exportable
*/
void
libsqfs_image_options_set_exportable(libsqfs_image_options_t options, bool exportable);

/**
	\brief Control padding
	\param options Handle for options
	\param padding Control whether file system image is padded
	
	Filesystems must be padded to a multiple of 4096 bytes if they
	are to be loop-back mounted. Except for debugging purposes it is
	generally advisable to leave this flag unchanged.
*/
void
libsqfs_image_options_set_padding(libsqfs_image_options_t options, bool padding);

/**
	\brief Control generation of fragment blocks
	\param options Handle for options
	\param fragments Strategy to use for generation of fragment blocks
*/
void
libsqfs_image_options_set_fragment_option(libsqfs_image_options_t options, libsqfs_fragments_option fragments);

/**
	\brief Control block size
	\param options Handle for options
	\param block_size Size of file data blocks
	
	The block size must be a power of 2 between 4096 and 1048576, any other value
	is silently ignored.
*/
void
libsqfs_image_options_set_block_size(libsqfs_image_options_t options, size_t block_size);

/*@}*/

/**
	\brief @c squashfs image handle
	
	Represents one @c squashfs image that is currently being processed.
*/
typedef struct _libsqfs_image * libsqfs_image_t;

/**
	\brief State of image
*/
typedef enum {
	/** \brief Image is currently being built */
	libsqfs_image_building,
	/** \brief An error occured while building image; image cannot be finalized anymore */
	libsqfs_image_fatal_error,
	/** \brief Image was finalized, i.e. fully written to destination */
	libsqfs_image_finalized
} libsqfs_image_state_t;

/**
	\brief Create @c squashfs image
	\param destination Output destination descriptor
	\param options Options controlling on-disk layout of image (or NULL for defaults)
	\return @c squashfs image handle, or NULL on error with errno set appropriately
	
	Create new image. @p options may either be NULL (in which case library defaults
	will be used for all settings), or a handle to an options data structure
	(created by \ref libsqfs_image_options_create and modified through the various
	calls descriebed in \ref image_options. In the latter case, the options will
	be copied over into the image, so the caller may call
	\ref libsqfs_image_options_destroy on it immediately afterwards.
*/
libsqfs_image_t
libsqfs_image_create(libsqfs_destination_t destination, libsqfs_image_options_t options);

/**
	\defgroup threads Multi-processing support
	
	Support for utilizing multiple processors for the tasks
	required in image creation (reading data, compression, writing
	data to the image).
*/
/*@{*/
/**
	\brief Spawn worker threads
	\param image @c squashfs image handle
	\param count Number of threads to be spawned
	\return Number of threads actually created
	
	Spawns a number of helper threads that will perform I/O and
	compression. The threads will automatically be terminated as
	soon as the image is closed.
*/
ssize_t
libsqfs_image_spawn_threads(libsqfs_image_t image, size_t count);

/**
	\brief Spawn worker threads
	\param image @c squashfs image handle
	\return Number of threads actually created
	
	Spawns a number of helper threads that will perform I/O and
	compression. The threads will automatically be terminated as
	soon as the image is closed. The number of threads spawned
	depends on the number of physical CPUs found in the system
	and/or system configuration parameters.
*/
ssize_t
libsqfs_image_auto_spawn_threads(libsqfs_image_t image);

/**
	\brief Assist in creating squashfs image
	\param image @c squashfs image handle
	
	The calling thread will assist in creating the squashfs image,
	performing compression and I/O operations on behalf of the
	main controlling thread. The function returns as soon as
	there is no more work to do (caused by e.g.
	\ref libsqfs_image_finalize or \ref libsqfs_image_abort).
	
	This provides a mechanism for an application to utilize
	multiple threads in creating squashfs images, but want
	to have control over thread creation and termination.
	
	<B>Important</B>: The main thread <B>must not</B> call
	\ref libsqfs_image_close before every other thread has
	returned from this call and must perform any required
	thread synchronization to achieve this. The main
	thread <B>must not</B> call this function itself
	as it will deadlock.
*/
void
libsqfs_image_worker_thread_function(libsqfs_image_t image);

/*@}*/

/**
	\brief Query state of @c squashfs image
	\param image @c squashfs image handle
	\return State of image
	
	While the image is in \ref libsqfs_image_building state, data
	may be added to the image. When the image is in
	\ref libsqfs_image_fatal_error state, a non-recoverable error occured
	while creating the image. The (partial) image already created
	is unusable and cannot be completed. When the image is in
	\ref libsqfs_image_finalized state, the image was completed
	successfully.
*/
libsqfs_image_state_t
libsqfs_image_state(libsqfs_image_t image);

/**
	\brief Finalize @c squashfs image
	\param image @c squashfs image handle
	\return Either \ref libsqfs_image_fatal_error or \ref libsqfs_image_finalized
	
	Finalizes the image, i.e. completes all pending writes and
	links internal structures. Nothing can be added to the image
	afterwards.
*/
libsqfs_image_state_t
libsqfs_image_finalize(libsqfs_image_t image);

/**
	\brief Abort creation of @c squashfs image
	\param image @c squashfs image handle
	
	Abort creation of image, cancels all pending operations.
	The image will be in \ref libsqfs_image_fatal_error state
	afterwards. Call this function if some error external
	to libsqfs occured in user code and you want to abort
	image creation as quickly as possible.
*/
void
libsqfs_image_abort(libsqfs_image_t image);

/**
	\brief Close @c squashfs image
	\param image @c squashfs image handle
	\return State of image after (attempted) finalization
	
	Closes the image and releases all resources acquired
	during construction of the image. Implicitly
	calls \ref libsqfs_image_finalize if the image is
	in \ref libsqfs_image_building state.
	
	The function waits for termination of all helper
	threads spawned through \ref libsqfs_image_spawn_threads
	or \ref libsqfs_image_auto_spawn_threads.
	
	If any user-thread has been assigned to assist in image
	creation through \ref libsqfs_image_worker_thread_function,
	you must ensure that all threads have returned from this
	call before closing the image.
*/
libsqfs_image_state_t
libsqfs_image_close(libsqfs_image_t image);

/**
	\brief Retrieve description of error
	\param image @c squashfs image handle
	\return String describing error
	
	Returns a string describing the first error that happened
	since beginning creation of the image or the last time
	the error state was cleared (see \ref libsqfs_image_error_clear).
	The string is allocated with the image
	and will be freed on both \ref libsqfs_image_close or
	\ref libsqfs_image_error_clear, the caller
	must thus copy it if it needs to be retained past this
	point.
*/
const char *
libsqfs_image_error_message(libsqfs_image_t image);

/**
	\brief Clear last error
	\param image @c squashfs image handle
	
	Clears the last error. Image creation may be proceed unless
	the error isn fatal (see \ref libsqfs_image_fatal_error).
*/
void
libsqfs_image_error_clear(libsqfs_image_t image);

/*@}*/

/**
	\defgroup inodes Inodes
	
	File-system objects (aka "inodes")
*/
/*@{*/

/**
	\defgroup inode_attrs Inode attributes
	
	Various attributes that may be attached to an inode
*/
/*@{*/

/**
	\brief Inode attributes
	
	Describes the attributes of an inode (e.g. owner, group, creation
	time, named extended attributes).
*/
typedef struct _libsqfs_inodeattr * libsqfs_inodeattr_t;

/**
	\brief Create inode attribute set
	\param image @c squashfs image handle
	\param uid uid
	\param gid gid
	\param mode File mode
	\param ctime Creation time
	\return Attribute handle, or NULL on failure with errno set appropriately
*/
libsqfs_inodeattr_t
libsqfs_inodeattr_create_simple(libsqfs_image_t image, uid_t uid, gid_t gid, mode_t mode, time_t ctime);

#if 0
/**
	\brief Extended attribute set
*/
typedef struct _libsqfs_xattrs * libsqfs_xattrs_t;
/**
	\brief Create extended attribute set
	\param image @c squashfs image handle
	\return Extended attribute handle, or NULL on failure with errno set appropriately
*/
libsqfs_xattrs_t
libsqfs_xattrs_create(libsqfs_image_t image);

/**
	\brief Add named extended attribute
	\param xattr Extended attribute set
	\param name Name of attribute
	\param value Buffer containing extended attribute data
	\param value_size size of extended attribute data
	\return @c true on success
*/
bool
libsqfs_xattrs_add_attribute(libsqfs_xattrs_t xattr, const char * name, const void * value, size_t value_size);

/**
	\brief Create inode attribute set
	\param image @c squashfs image handle
	\param uid uid
	\param gid gid
	\param mode File mode
	\param ctime Creation time
	\param xattr Extended attributes
	\return Attribute handle, or NULL on failure with errno set appropriately
*/
libsqfs_inodeattr_t
libsqfs_inodeattr_create_extended(libsqfs_image_t image, uid_t uid, gid_t gid, mode_t mode, time_t ctime, libsqfs_xattrs_t xattr);

#endif

/*@}*/

/**
	\brief Base handle type for all filesystem objects
*/
typedef struct _libsqfs_inode * libsqfs_inode_t;

/**
	\defgroup regular_inodes Regular (aka "file") inodes
*/
/*@{*/

/**
	\defgroup regular_inode_data File data
*/
/*@{*/

/**
	\brief File data content handle
*/
typedef struct _libsqfs_data * libsqfs_data_t;

/**
	\brief Create file data
	\param image @c squashfs image handle
	\param srcpath Path of file with data to import
	\return Data handle, or NULL on failure
	
	Creates a handle that represents the data contained in the specified
	file.
*/
libsqfs_data_t
libsqfs_data_create_from_file(libsqfs_image_t image, const char * srcpath);

/**
	\brief Create file data
	\param image @c squashfs image handle
	\param buffer An in-memory buffer containing the data
	\param size Size of the in-memory buffer
	\param deleter Function to be called when the buffer is no longer needed
	\param deleter_closure Parameter passed to @c deleter function
	
	Creates a handle that represents the data contained in the specified
	buffer. When the buffer is no longer needed, the specified deleter
	will be called to perform user-defined cleanup actions.
*/
libsqfs_data_t
libsqfs_data_create_for_buffer(libsqfs_image_t image, const void * buffer, size_t size, void (*deleter)(void *), void * deleter_closure);

/**
	\brief Create file data
	\param image @c squashfs image handle
	\param buffer An in-memory buffer containing the data
	\param size Size of the in-memory buffer
	
	Creates a handle that represents the data contained in the specified
	buffer. The buffer is assumed to be "static", i.e. the caller is responsible
	for cleaning it up. The only safe point in time to do that is after closing
	the @c squashfs image handle the data element is associated with it.
*/
libsqfs_data_t
libsqfs_data_create_for_static_buffer(libsqfs_image_t image, const void * buffer, size_t size);

/**
	\brief Create file data
	\param image @c squashfs image handle
	\param buffer An in-memory buffer containing the data
	\param size Size of the in-memory buffer
	
	Creates a handle that represents the data contained in the specified
	buffer. @c libsqfs takes ownership of the passed buffer and will
	call @c free on the buffer when it is no longer needed.
*/
libsqfs_data_t
libsqfs_data_create_for_transferred_buffer(libsqfs_image_t image, void * buffer, size_t size);

/**
	\brief Piece of a data element
	
	Represents a "piece" of a larger data element
*/
typedef struct _libsqfs_data_piece {
	/** \brief Referenced data piece */
	libsqfs_data_t data;
	/** \brief Size of the piece */
	unsigned long long size;
	/** \brief Offset of the piece */
	unsigned long long offset;
} libsqfs_data_piece;

/**
	\brief Create file data
	\param image @c squashfs image handle
	\param npieces Number of pieces
	\param pieces Pieces of data
	
	Creates a handle that represents the data composed of the
	specified individual pieces.
*/
libsqfs_data_t
libsqfs_data_create_compound(libsqfs_image_t image, size_t npieces,
	const libsqfs_data_piece pieces[]);

/*@}*/

/**
	\brief Regular ("file") inode handle
*/
typedef struct _libsqfs_regular_inode * libsqfs_regular_inode_t;

/**
	\brief Create regular ("file") inode
	\param image @c squashfs image handle
	\param attr Attributes
	\param data Data to be contained in file
	\return inode handle, or NULL on failure
	
	Note: It is possible (and legal) to reuse the same attributes and/or
	data for multiple files. libsqfs will make sure the data is written
	to the image only once.
*/
libsqfs_regular_inode_t
libsqfs_regular_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, libsqfs_data_t data);

/**
	\brief Reinterpret regular ("file") as generic inode
	\param inode File inode
	\return The same inode, reinterpreted as generic inode
*/
libsqfs_inode_t
libsqfs_regular_inode_downcast(libsqfs_regular_inode_t inode);

/*@}*/

/**
	\defgroup directory_inodes Directory inodes
*/
/*@{*/

/**
	\brief Directory inode
*/
typedef struct _libsqfs_directory_inode * libsqfs_directory_inode_t;

/**
	\brief Create directory inode
	\param image @c squashfs image handle
	\param attr Attributes
	\return inode handle, or NULL on failure
*/
libsqfs_directory_inode_t
libsqfs_directory_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr);

/**
	\brief Add entry to directory
	\param parent Directory to which entry will be added
	\param name Name ("file name") of directory entry
	\param inode Inode to which directory entry should point
	\return @c true on success
	
	Adds named entry to directory.
	It is possible (and legal) to create multiple names for the
	same inode (equivalent to multiple "hard links"), but hard-linked
	directories are disallowed.
*/
bool
libsqfs_directory_add_entry(libsqfs_directory_inode_t parent, const char * name, libsqfs_inode_t inode);

/**
	\brief Reinterpret directory as generic inode
	\param inode Directory inode
	\return The same inode, reinterpreted as generic inode
*/
libsqfs_inode_t
libsqfs_directory_inode_downcast(libsqfs_directory_inode_t inode);

/*@}*/

/**
	\defgroup symlink_inodes Symlink inodes
*/
/*@{*/

/**
	\brief Symlink inode
*/
typedef struct _libsqfs_symlink_inode * libsqfs_symlink_inode_t;

/**
	\brief Create symlink inode
	\param image @c squashfs image handle
	\param attr Attributes
	\param target Target of symbolic link
	\return inode handle, or NULL on failure
*/
libsqfs_symlink_inode_t
libsqfs_symlink_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, const char target[]);

/**
	\brief Reinterpret symlink as generic inode
	\param inode Symlink inode
	\return The same inode, reinterpreted as generic inode
*/
libsqfs_inode_t
libsqfs_symlink_inode_downcast(libsqfs_symlink_inode_t inode);

/*@}*/

/**
	\defgroup device_inodes Device inodes
*/
/*@{*/

/**
	\brief Device inode
*/
typedef struct _libsqfs_device_inode * libsqfs_device_inode_t;

/**
	\brief Create device inode
	\param image @c squashfs image handle
	\param attr Attributes
	\param type Either 'c' or 'b' to indicate char or block device
	\param dev_major_no Major device number
	\param dev_minor_no Minor device number
	\return inode handle, or NULL on failure
*/
libsqfs_device_inode_t
libsqfs_device_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr, char type, unsigned int dev_major_no, unsigned int dev_minor_no);

/**
	\brief Reinterpret device as generic inode
	\param inode Symlink inode
	\return The same inode, reinterpreted as generic inode
*/
libsqfs_inode_t
libsqfs_device_inode_downcast(libsqfs_device_inode_t inode);

/*@}*/

/*@}*/

typedef struct _libsqfs_fifo_inode * libsqfs_fifo_inode_t;

libsqfs_fifo_inode_t 
libsqfs_fifo_inode_create(libsqfs_image_t image, libsqfs_inodeattr_t attr);

libsqfs_inode_t
libsqfs_fifo_inode_downcast(libsqfs_fifo_inode_t inode);

/** \addtogroup images */
/*@{*/

/**
	\brief Set root directory inode
	\param image @c squashfs image handle
	\param root root directory
	\return @c true on success
	
	Sets the root directory of the @c squashfs image. Must be called exactly
	once for every image.
*/
bool
libsqfs_image_set_root(libsqfs_image_t image, libsqfs_directory_inode_t root);

/**
	\brief Retrieve root directory inode
	\param image @c squashfs image handle
	\return Handle for root directory inode; may be NULL if none has been set yet
*/
libsqfs_directory_inode_t
libsqfs_image_get_root(libsqfs_image_t image);

/*@}*/

#ifdef __cpluscplus
}
#endif

#endif
