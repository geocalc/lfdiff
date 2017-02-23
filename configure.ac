#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([lfdiff], [0.9], [jh@mwerk.net])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([src/diffmanager.c])
AC_CONFIG_MACRO_DIRS([m4])

# Checks for programs.
AC_PROG_CC
AM_PROG_AR
AC_PROG_LIBTOOL
# used by ltmain.sh
AC_PROG_CXX
AC_PROG_RANLIB

# Checks for libraries.

# Use pkg-config instead of upstream-provided and possibly-broken
# AM_PATH_CHECK. This command sets CHECK_CFLAGS and CHECK_LIBS
# appropriately.
# TODO test for "check" only if --enable-test is provided on the command line
AC_ARG_ENABLE([test],
	[AS_HELP_STRING([--enable-test],
		[enable unit tests, i.e. make check [default=no]])],
	[],
	[enable_test=no])

AS_IF([test "x$enable_test" != xno],
            [PKG_CHECK_MODULES([CHECK], [check])])

# check for pthreads
# see https://airtower.wordpress.com/2010/06/14/autotools-and-pthreads/
# and http://www.gnu.org/software/autoconf-archive/ax_pthread.html
# note gentoo: using sys-devel/autoconf-archive
AX_PTHREAD
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_ASSERT
AC_CHECK_HEADERS([limits.h stdlib.h string.h strings.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([dup2 memset regcomp strdup strerror])

# Output files
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile src/Makefile tests/Makefile])
AC_OUTPUT
