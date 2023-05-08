#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([genext2fs], [1.4.2])
AC_CONFIG_SRCDIR([genext2fs.c])

builtin(include, [m4/ac_func_snprintf.m4])dnl
builtin(include, [m4/ac_func_scanf_can_malloc.m4])dnl

AM_INIT_AUTOMAKE([foreign])
AC_CONFIG_HEADER([config.h])

AC_GNU_SOURCE

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_MAJOR
AC_CHECK_HEADERS([fcntl.h inttypes.h limits.h memory.h stddef.h stdint.h stdlib.h string.h strings.h unistd.h])
AC_CHECK_HEADERS([libgen.h getopt.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_C_INLINE
AC_CHECK_TYPE(size_t, unsigned)
AC_CHECK_TYPE(ssize_t, signed)
AC_CHECK_MEMBERS([struct stat.st_rdev])

# Checks for library functions.
AC_CHECK_FUNCS([getopt_long getline strtof])
AC_FUNC_SNPRINTF
AC_FUNC_SCANF_CAN_MALLOC

AC_MSG_CHECKING(--enable-libarchive argument)
AC_ARG_ENABLE(libarchive,
    [  --enable-libarchive        Include libarchive support.],
    [enable_libarchive=$enableval],
    [enable_libarchive="yes"])
AC_MSG_RESULT($enable_libarchive)
if test "$enable_libarchive" = "yes"; then
# Check for libarchive (no pkg-config support)
AC_CHECK_LIB([archive], [archive_read_new], [ARCHIVE_LIBS=-larchive],
    AC_MSG_ERROR([libarchive not found.
                  If libarchive is installed then perhaps you should set
                  the LDFLAGS=-L/nonstandard/lib/dir environment variable]))
AC_SUBST([ARCHIVE_LIBS])
AC_CHECK_HEADERS([archive.h archive_entry.h],,
    AC_MSG_ERROR([libarchive headers not found.
                  If the libarchive headers are installed then perhaps you
                  should set the CPPFLAGS=-I/nonstandard/include/dir
                  environment variable]))
AC_DEFINE([HAVE_LIBARCHIVE], [], [Description])
fi

AC_OUTPUT([Makefile],[
chmod a+x $ac_top_srcdir/test-mount.sh $ac_top_srcdir/test.sh
])
