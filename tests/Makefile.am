AM_CFLAGS = -I$(top_srcdir)/include $(CWARNFLAGS)

libsqfs = $(top_builddir)/src/libsqfs.la

check_PROGRAMS = test-options test-empty-image test-simple-file test-special-files test-inode-mode test-dir-layout test-file-xattrs compounddata

test_options_SOURCES = test-options.c
test_options_LDADD = $(libsqfs)

test_dir_layout_SOURCES = test-dir-layout.c
test_dir_layout_LDADD = $(libsqfs)

test_empty_image_SOURCES = test-empty-image.c
test_empty_image_LDADD = $(libsqfs)

test_file_xattrs_SOURCES = test-file-xattrs.c
test_file_xattrs_LDADD = $(libsqfs)

test_simple_file_SOURCES = test-simple-file.c
test_simple_file_LDADD = $(libsqfs)

test_special_files_SOURCES = test-special-files.c
test_special_files_LDADD = $(libsqfs)

test_inode_mode_SOURCES = test-inode-mode.c
test_inode_mode_LDADD = $(libsqfs)

compounddata_SOURCES = compounddata.c
compounddata_LDADD = $(libsqfs)

if BUILD_LIBZSTD_COMPRESSOR
check_PROGRAMS += test-zstd

test_zstd_SOURCES = test-zstd.c
test_zstd_LDADD = $(libsqfs)
endif

TESTS = $(check_PROGRAMS)
