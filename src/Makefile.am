if BUILD_ZLIB_COMPRESSOR
MAYBE_ZLIB_COMPRESSOR = compressor_zlib.c
endif

if BUILD_LIBLZMA_COMPRESSOR
MAYBE_LIBLZMA_COMPRESSOR = compressor_liblzma.c compressor_xz.c
endif

if BUILD_LIBZSTD_COMPRESSOR
MAYBE_LIBZSTD_COMPRESSOR = compressor_zstd.c
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
	$(MAYBE_LIBLZMA_COMPRESSOR) \
	$(MAYBE_LIBZSTD_COMPRESSOR) \
	threadpool.c

libsqfs_la_CFLAGS = $(AM_CFLAGS) $(ZLIB_CFLAGS) $(LIBLZMA_CFLAGS) $(LIBZSTD_CFLAGS)
libsqfs_la_LDFLAGS = -version-number 0:1:0 -no-undefined
libsqfs_la_LIBADD = -lpthread $(ZLIB_LIBS) $(LIBLZMA_LIBS) $(LIBZSTD_LIBS)
libsqfsincludedir = $(includedir)
libsqfsinclude_HEADERS = $(top_srcdir)/include/libsqfs.h $(top_srcdir)/include/libsqfs/site-config.h
