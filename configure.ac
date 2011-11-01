AC_INIT([libsqfs], [0.1], [helge.bahmann@secunet.com])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER(src/config.h)
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

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 examples/Makefile
                 libsqfs.pc])
AC_OUTPUT
