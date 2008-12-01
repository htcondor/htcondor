#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# XXX: This configure script does not allow for the use of stlport or
# changing the library name (no --lib-name <name>). -matt 080607

# NOTES:
#  * AS_HELP_STRING is AC_HELP_STRING in 2.57
#  * AC_CONFIG_AUX_DIR in 2.57 assumes the dir already exists
#  * -Wall -Werror for AM_INIT_AUTOMAKE are not recognized in autoconf 2.57

# Find valid warning flags for the C Compiler.           -*-Autoconf-*-
dnl Copyright (C) 2001, 2002, 2006 Free Software Foundation, Inc.
dnl This file is free software; the Free Software Foundation
dnl gives unlimited permission to copy and/or distribute it,
dnl with or without modifications, as long as this notice is preserved.
dnl Written by Jesse Thilo.

AC_DEFUN([gl_COMPILER_FLAGS],
  [AC_MSG_CHECKING(whether compiler accepts $1)
   AC_SUBST(COMPILER_FLAGS)
   ac_save_CFLAGS="$CFLAGS"
   CFLAGS="$CFLAGS $1"
   ac_save_CXXFLAGS="$CXXFLAGS"
   CXXFLAGS="$CXXFLAGS $1"
   AC_TRY_COMPILE(,
    [int x;],
    COMPILER_FLAGS="$COMPILER_FLAGS $1"
    AC_MSG_RESULT(yes),
    AC_MSG_RESULT(no))
  CFLAGS="$ac_save_CFLAGS"
  CXXFLAGS="$ac_save_CXXFLAGS"
 ])


AC_PREREQ(2.57)
AC_INIT([classad], [1.0-rc5], [condor-admin@cs.wisc.edu])
#AC_CONFIG_AUX_DIR([build-aux])
#AM_INIT_AUTOMAKE([1.6.3 -Wall -Werror foreign])
AM_INIT_AUTOMAKE([1.6.3 foreign])
AC_CONFIG_SRCDIR([classad.cpp])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_FILES([Makefile])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL
AC_LANG([C++])

# Check for compiler flags.
#gl_COMPILER_FLAGS([-Werror])
#gl_COMPILER_FLAGS([-pedantic])
gl_COMPILER_FLAGS([-Wall])
gl_COMPILER_FLAGS([-Wextra])
gl_COMPILER_FLAGS([-Wshadow])
gl_COMPILER_FLAGS([-Wpointer-arith])
gl_COMPILER_FLAGS([-Wcast-qual])
gl_COMPILER_FLAGS([-Wcast-align])
gl_COMPILER_FLAGS([-Winvalid-pch])
gl_COMPILER_FLAGS([-Woverloaded-virtual])
gl_COMPILER_FLAGS([-Wno-system-headers])
CXXFLAGS="$COMPILER_FLAGS $CXXFLAGS"

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h float.h limits.h stdlib.h string.h sys/time.h sys/timeb.h unistd.h regex.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

AC_CHECK_MEMBER([struct tm.tm_gmtoff],
                [AC_DEFINE([HAVE_TM_GMTOFF], [1], [tm.tm_gmtoff available])],
                [],
                [[#include <time.h>]])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE

# AC_FUNC_MALLOC defines malloc to rpl_malloc on systems where malloc(0)
# returns NULL.  Unfortunately, this conflicts with some STL headers on
# ppc_aix_5.2 (gcc 3.3.2).  Since we are not aware of any case where
# classads cares about malloc(0), we therefore are disabling this check
# on all platforms.
#AC_FUNC_MALLOC

AC_FUNC_MKTIME
AC_FUNC_STRFTIME
AC_FUNC_STRTOD
AC_FUNC_VPRINTF
AC_CHECK_FUNCS([floor ftime getcwd gettimeofday localtime_r memset regcomp rint strcasecmp strdup strtol tzset])
AC_CHECK_FUNC(dlopen,
  AC_DEFINE([HAVE_DLOPEN], [], [dlopen available]),
  AC_CHECK_LIB(dl, dlopen,
    [LIBS="$LIBS -ldl"
     AC_DEFINE([HAVE_DLOPEN], [], [dlopen available])]))
AC_CHECK_FUNC([regcomp], [HAVE_POSIX_REGEX=yes])

# On HPUX11, localtime_r() and gmtime_r() are not defined unless
# _REENTRANT is defined.  Test for that case here.

AC_MSG_CHECKING([if localtime_r() needs _REENTRANT])
AC_LANG(C++)
AC_COMPILE_IFELSE([#include <time.h>
                   void testfunc() {localtime_r(NULL,NULL);}],[
		   AC_MSG_RESULT([no])],[
                   AC_COMPILE_IFELSE([
                     #define _REENTRANT
                     #include <time.h>
                     void testfunc() {localtime_r(NULL,NULL);}],[
		     AC_MSG_RESULT([yes])
                     AC_DEFINE([_REENTRANT],[1],[needed for localtime_r() and gmtime_r()])],[
                   AC_MSG_RESULT([test failed])])])

# Checks for libraries.
AC_ARG_WITH([pcre],
  AC_HELP_STRING([--with-pcre@<:@=DIR@:>@],
    [Use PCRE instead of POSIX regex. The directory should be include and lib subdirectories (default: not used)]))

AS_IF([test "x$with_pcre" != xno -a "x$with_pcre" != xyes -a "x$with_pcre" != x],
  [pcre_dir=$with_pcre])

AS_IF([test "x$with_pcre" != xno],
  [_ldflags_save=$LDFLAGS
   _cppflags_save=$CPPFLAGS
   AS_IF([test "x$pcre_dir" != x],
     [LDFLAGS="$LDFLAGS -L$pcre_dir/lib"
      CPPFLAGS="$CPPFLAGS -I$pcre_dir/include"])
   AC_CHECK_HEADERS([pcre.h pcre/pcre.h])
   AC_CHECK_LIB([pcre], [pcre_compile],
     [LIBS="-lpcre $LIBS"
      CPPFLAGS="$CPPFLAGS -DUSE_PCRE"],
     [LDFLAGS=$_ldflags_save
      AS_IF([test "x$HAVE_POSIX_REGEX" = xyes],
        [CPPFLAGS="$_cppflags_save -DUSE_POSIX_REGEX"],
	[CPPFLAGS=$_cppflags_save])])],
  [AS_IF([test "x$HAVE_POSIX_REGEX" = xyes],
     [CPPFLAGS="$CPPFLAGS -DUSE_POSIX_REGEX"])])


# Check for other things, such as --enable arguments

AC_ARG_ENABLE([explicit-templates],
  AC_HELP_STRING([--enable-explicit-tempaltes],
    [Use explicit template instantiation, not implicit (default: no)]))

AM_CONDITIONAL([ENABLE_EXPLICIT_TEMPLATES],
  [test "x$enable_explicit_templates" = xyes])

AS_IF([test "x$enable_explicit_templates" = xyes],
  [CPPFLAGS="$CPPFLAGS -fno-implicit-templates"])


#AC_ARG_ENABLE([shlib],
#  AC_HELP_STRING([--enable-shlib],
#    [Allow user functions in shared libraries (default: no)]))
#
#AM_CONDITIONAL([ENABLE_SHLIB], [test "x$enable_shlib" = xyes])


AC_ARG_ENABLE([namespace],
  AC_HELP_STRING([--enable-namespace],
    [Use the classad namespace (default: no)]))

AM_CONDITIONAL([ENABLE_NAMESPACE], [test "x$enable_namespace" = xyes])

AC_ARG_ENABLE([dlopen-check],
  AC_HELP_STRING([--enable-dlopen-check],
    [Test dlopen (default: no)]))
AM_CONDITIONAL([ENABLE_DLOPEN_CHECK], [test "x$enable_dlopen_check" = xyes])
AS_IF([test "x$enable_dlopen_check" != xyes],[AC_DEFINE([DISABLE_DLOPEN_CHECK], [1], [testing of dlopen disabled])])

AC_ARG_ENABLE([flexible-member],
  AC_HELP_STRING([--enable-flexible-member],
    [Allow member function to be (list, X) or (X, list) (default: no]))

AS_IF([test "x$enable_flexible_member" = xyes],
  [CPPFLAGS="$CPPFLAGS -DFLEXIBLE_MEMBER"])


CPPFLAGS="$CPPFLAGS -DCLASSAD_DISTRIBUTION"

AC_OUTPUT

# Remove rpath junk from libtool
sed 's|^hardcode_libdir_flag_spec=.*|hardcode_libdir_flag_spec=""|g
     s|^runpath_var=LD_RUN_PATH|runpath_var=DIE_RPATH_DIE|g' libtool > libtool.new && mv libtool.new libtool
