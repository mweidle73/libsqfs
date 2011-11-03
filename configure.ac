AC_INIT([libsqfs], [0.1], [helge.bahmann@secunet.com])
AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADERS([src/config.h include/libsqfs/compressors.h])
AM_MAINTAINER_MODE
AC_SYS_LARGEFILE
AC_PROG_CC
AC_PROG_CC_C99
AC_PROG_LIBTOOL
AC_CHECK_FUNCS(pread pwrite)

AC_DEFINE([_XOPEN_SOURCE], [500], [Required definition for pread/pwrite])

AM_CFLAGS="-Wall -Werror"
CWARNFLAGS="-Wall -Werror"
AC_SUBST(CWARNFLAGS)

AC_ARG_ENABLE(examples, AS_HELP_STRING([--enable-examples], [Build example programs (default: enabled)]),
	[ENABLE_EXAMPLES=$enableval], [ENABLE_EXAMPLES=yes])
AM_CONDITIONAL([BUILD_EXAMPLES], [test "x$ENABLE_EXAMPLES" = "xyes"])

PKG_CHECK_MODULES([ZLIB], [zlib], [HAVE_ZLIB=yes], [HAVE_ZLIB=no])
AM_CONDITIONAL([BUILD_ZLIB_COMPRESSOR], [test "x$HAVE_ZLIB" = "xyes"])

PKG_CHECK_MODULES([LIBLZMA], [liblzma], [HAVE_LIBLZMA=yes], [HAVE_LIBLZMA=no])
AM_CONDITIONAL([BUILD_LIBLZMA_COMPRESSOR], [test "x$HAVE_LIBLZMA" = "xyes"])

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 examples/Makefile
                 libsqfs.pc])

AC_CONFIG_COMMANDS([include/libsqfs/site-config.h],[
  outfile=include/libsqfs/site-config.h
  tmpfile=${outfile}T
  echo creating $outfile
  cat > $tmpfile << _EOF_
#ifndef LIBSQFS_SITE_CONFIG_H
#define LIBSQFS_SITE_CONFIG_H

_EOF_
  if test x$HAVE_ZLIB = xyes ; then
    echo '#define LIBSQFS_HAVE_COMPRESSOR_ZLIB 1' >> $tmpfile
  fi
  if test x$HAVE_LIBLZMA = xyes ; then
    echo '#define LIBSQFS_HAVE_COMPRESSOR_LZMA 1' >> $tmpfile
  fi
cat >> $tmpfile << _EOF_

#endif
_EOF_
  mv $tmpfile $outfile
],[
  HAVE_ZLIB=$HAVE_ZLIB
  HAVE_LIBLZMA=$HAVE_LIBLZMA
])
AC_OUTPUT
