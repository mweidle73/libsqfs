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

AC_CONFIG_FILES([Makefile
                 src/Makefile
                 libsqfs.pc])
AC_OUTPUT
