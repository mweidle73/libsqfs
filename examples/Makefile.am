AM_CFLAGS = -I$(top_srcdir)/include $(CWARNFLAGS)

libsqfs = $(top_builddir)/src/libsqfs.la

bin_PROGRAMS = mksqfs

mksqfs_SOURCES = mksqfs.c
mksqfs_LDADD = $(libsqfs)
