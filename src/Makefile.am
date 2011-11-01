if BUILD_ZLIB_COMPRESSOR
MAYBE_ZLIB_COMPRESSOR = compressor_zlib.c
endif

AM_CFLAGS = -I$(top_srcdir)/include $(CWARNFLAGS)

lib_LTLIBRARIES = libsqfs.la

libsqfs_la_SOURCES = \
	destination.c \
	image.c \
	inodes.c \
	superblock.c \
	idtable.c \
	metatable.c \
	directory.c \
	datasource.c \
	bulkdata.c \
	regular.c \
	special.c \
	export.c \
	xattrs.c \
	compressor.c \
	$(MAYBE_ZLIB_COMPRESSOR) \
	threadpool.c

libsqfs_la_CFLAGS = $(ZLIB_CFLAGS) $(AM_CFLAGS)
libsqfs_la_LDFLAGS = -version-number 0:1:0 -no-undefined
libsqfs_la_LIBADD = -lpthread $(ZLIB_LIBS)
libsqfsincludedir = $(includedir)
libsqfsinclude_HEADERS = $(top_srcdir)/include/libsqfs.h
