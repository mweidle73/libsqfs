#ifndef LIBSQFS_H
#define LIBSQFS_H

#include <stdbool.h>
#include <sys/types.h>

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

/*@}*/

/**
	\defgroup images @c squashfs images
*/
/*@{*/

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
	/** \brief An error occured while building image; image cannot be finilazed anymore */
	libsqfs_image_error,
	/** \brief Image was finalized, i.e. fully written to destination */
	libsqfs_image_finalized
} libsqfs_image_state_t;

/**
	\brief Create @c squashfs image
	\param destination Output destination descriptor
	\return @c squashfs image handle, or NULL on error with errno set appropriately
	
	Create new image
*/
libsqfs_image_t
libsqfs_image_create(libsqfs_destination_t destination);

/**
	\brief Query state of @c squashfs image
	\param image @c squashfs image handle
	\return State of image
	
	While the image is in \ref libsqfs_image_building state, data
	may be added to the image. When the image is in
	\ref libsqfs_image_error state, a non-recoverable error occured
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
	\return Either \ref libsqfs_image_error or \ref libsqfs_image_finalized
	
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
	The image will be in \ref libsqfs_image_error state
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
*/
libsqfs_image_state_t
libsqfs_image_close(libsqfs_image_t image);

/**
	\brief Retrieve description of error
	\param image @c squashfs image handle
	\return String describing error
	
	Returns a string describing the error that happened while
	creating the image. The string is allocated with the image
	and will be freed on \ref libsqfs_image_close, the caller
	must thus copy it if it needs to be retained past this
	point.
*/
const char *
libsqfs_image_errorstr(libsqfs_image_t image);

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
	\brief Regular ("file") inode handle
*/
typedef struct _libsqfs_regular_inode * libsqfs_regular_inode_t;

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
	same inode (equivalent to multiple "hard links"). It is also possible
	to create hard-linked directories, but the caller is responsible
	for ensuring that no directory loops are created.
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

/*@}*/

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

#endif
