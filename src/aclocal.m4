dnl Using mf_ for non Autoconf macros.
m4_pattern_forbid([^mf_]])dnl
m4_pattern_forbid([^MF_])dnl


#######################################################################
# CHECK_PROG_IS_GNU is based loosely on the CHECK_GNU_MAKE macro 
# available from the GNU Autoconf Macro Archive at:
# http://www.gnu.org/software/ac-archive/htmldoc/check_gnu_make.html
# This version is more generic in that it checks any given program if
# it's GNU or not.  Written by Derek Wright <wright@cs.wisc.edu>
# Arguments:
#  $1 is the PATH of the program to test for
#  $2 the variable name you want to use to see if it's GNU or not
# Side effects:
#  Sets _cv_<progname>_is_gnu to "yes" or "no" as appropriate
#######################################################################
AC_DEFUN( [CHECK_PROG_IS_GNU],
[AC_CACHE_CHECK( [if $1 is GNU], [_cv_$2_is_gnu],
 [if ( sh -c "$1 --version" 2> /dev/null | grep GNU > /dev/null 2>&1 ) ;
  then
     _cv_$2_is_gnu=yes
  else
     _cv_$2_is_gnu=no
  fi
 ])
])


AC_DEFUN( [CHECK_TAR_OPTION],
 [AC_CACHE_CHECK( [if tar supports $1], [$2],
  [( $_cv_gnu_tar -cf _cv_test_tar.tar $1 configure > /dev/null 2>&1 )
  _tar_status=$?
  rm -f _cv_test_tar.tar 2>&1 > /dev/null
  if test $_tar_status -ne 0; then
    $2="no";
  else
    $2="yes";
  fi
 ])
])


dnl Available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/ac_prog_perl_version.html
dnl
AC_DEFUN([AC_PROG_PERL_VERSION],[dnl
# Make sure we have perl
if test -z "$PERL"; then
  AC_PATH_PROG(PERL,perl,no,[$PATH])
fi
# Check if version of Perl is sufficient
ac_perl_version="$1"
if test "$PERL" != "no"; then
  AC_MSG_CHECKING(for perl version greater than or equal to $ac_perl_version)
  # NB: It would be nice to log the error if there is one, but we cannot rely
  # on autoconf internals
  $PERL -e "use $ac_perl_version;" > /dev/null 2>&1
  if test $? -ne 0; then
    AC_MSG_RESULT(no);
    $3
  else
    AC_MSG_RESULT(ok);
    $2
  fi
else
  AC_MSG_WARN(could not find perl)
fi
])dnl

dnl Check to see if perl supports a specific module
dnl first argument is module name
dnl second argument is the symbol you would like defined if true.
AC_DEFUN([AC_CHECK_PERL_MODULE],[dnl
# Make sure we have perl
if test -z "$PERL"; then
  AC_PATH_PROG(PERL,perl,no,[$PATH])
fi
# Check if version of Perl is sufficient
ac_perl_module="$1"
if test "$PERL" != "no"; then
  AC_MSG_CHECKING(for perl module $ac_perl_module)
  $PERL -e "use $ac_perl_module; exit 0;" > /dev/null 2>&1
  if test $? -ne 0; then
    AC_MSG_RESULT(no);
  else
    AC_MSG_RESULT(ok);
	AC_DEFINE([$2], [1], [Do we have the perl module $1?])
  fi
else
  AC_MSG_WARN(could not find perl)
fi
])dnl


#
# CHECK_EXTERNAL(name,
#                version,
#                requirement_level,
#                help string,
#                test)
#
# Check for an external. If $enable_proper is defined to "yes" perform
# the external check with the provided test function ($5), otherwise
# use the default check for an external directory. The help string
# ($4) is also ignored if $enable_proper is not "yes". _cv_has_<name>
# is checked to see if the external should detected. If _cv_has_<name>
# is no and the requirement is hard an error is raised.
#
# Example usage:
#  CHECK_EXTERNAL([pcre], [5.0], [hard],
#                 [use PCRE (provides regular expression support)],
#                 MF_LIB_CHECK([pcre], [[pcre pcre_compile]]))
#
# Arguments:
#  * name: The externals name, e.g. [pcre]
#  * version: The externals desired version, e.g. [5.0]
#  * requirement_level: [hard]|[soft]|[optional], should be [hard]|[soft]
#         [hard]: This external is a HARD requirement, it must exist
#         [soft]: This external is a SOFT requirement, it would be nice
#     [optional]: This external is an OPTIONAL requirement, who cares
#  * help string: Message provided with --with-<name> in --help
#  * test: A function to determine if the external is available. It
#          will have access to _dir, which is set if
#          --with-<name>[=DIR] is given a DIR. When the function
#          completes it must define cv_ext_<name>=yes|no depending on
#          if the external is available or not. If none is given the
#          external is assumed to not be available.
#
# Results:
#  * On successful location/identification of the external:
#   _cv_ext_<name>_version=<name>-<version> | NA
#   AC_SUBST([ext_<name>_version], [$_cv_ext_<name>_version])
#   AC_DEFINE([HAVE_EXT_<NAME>], [1])
#
AC_DEFUN([CHECK_EXTERNAL],
  [AS_IF([test "x$_cv_has_]m4_tolower($1)[" = xno],
     [AS_IF([test "x$3" = xhard],
       [AC_MSG_FAILURE([$1 required but unsupported])],
       [AC_MSG_CHECKING([for $1])
        AC_MSG_RESULT([unsupported])])],
     [AS_IF([test "x$enable_proper" = xyes],
       [MF_EXTERNAL_CHECK($1, $2, $4, $3, [$5])],
       [MF_EXTERNAL_CHECK($1, $2, $4, $3, CONDOR_EXTERNAL_VERSION($1, $2))])])])

#
# MF_EXTERNAL_CHECK(external_name,
#                   external_version,
#                   help string,
#                   requierment_level,
#                   test)
#
# This function is the framework for detecting externals under
# enable_proper, i.e. externals present on the system not in special
# directories waiting to be built. The test function ($5) does the
# work of actually detecting an external, while this function worries
# about the requirement_level ($4), if --with-<name> or
# --without-<name> is given, if --enable-soft-is-hard is given, and
# what should happen when the external is found or not.
#
# Arguments:
#  Exactly the same as those given to CHECK_EXTERNAL
#
# Results:
#  Exactly the same as CHECK_EXTERNAL's
#
AC_DEFUN([MF_EXTERNAL_CHECK],
  [AC_ARG_WITH(m4_tolower($1),
    AS_HELP_STRING([--with-]m4_tolower($1)[@<:@=DIR@:>@],
                   [$3 ($4 requirement)]),
    [_with=$withval],
    [_with=check])
   # if not yes|no|check --with-<name>[=DIR] was given a DIR
   AS_IF([test "x$_with" != xyes -a "x$_with" != xno -a "x$_with" != xcheck],
     [_dir=$_with],
     [_dir=])
   # if a hard requirement, it is always required
   # note: --without-<name> makes no sense on a hard requirement
   AS_IF([test "x$4" = xhard],
     [_required=yes],
     # if --with-<name> is given
     AS_IF([test "x$_with" != xcheck -a "x$_with" != xno],
       [_required=yes],
       # if a soft requirement
       AS_IF([test "x$4" = xsoft],
         # external is a soft requirement, but soft->hard turned on
         [AS_IF([test "x$enable_soft_is_hard" = xyes],
           [_required=yes],
           # else, just a soft requirement
           [_required=no])],
         # else, completely optional
         [_required=no])))
   # if --without-<name>
   AS_IF([test "x$_with" = xno],
     [cv_ext_]m4_tolower($1)[=no],
     [dnl invoke the test program or if it is not given just assume the
      dnl external is not available
      m4_if(x$5, x, [cv_ext_]m4_tolower($1)[=no], $5)])
   AC_MSG_CHECKING([for $1])
   AC_MSG_RESULT([$cv_ext_]m4_tolower($1))
   # if the external is not available
   AS_IF([test "x$cv_ext_]m4_tolower($1)[" = xno],
     [AS_IF([test "x$_required" = xyes],
       [AC_MSG_FAILURE([$1 is required])])],
     [# Strictly backwards compatible with CONDOR_EXTERNAL_VERSION
      _cv_ext_$1_version="$1-$2"
      AC_SUBST([ext_$1_version], [$_cv_ext_$1_version])
      AC_DEFINE([HAVE_EXT_]m4_toupper($1), [1], [Do we have the $1 external])])])


# Helper functions, basically car and cadr, pulling the first and
# second items out of a space separated pair, e.g. [first second]
AC_DEFUN([mf_first],
  [m4_normalize(m4_substr($1, 0, m4_index($1, [ ])))])
AC_DEFUN([mf_second],
  [m4_normalize(m4_substr($1,
                          m4_index($1, [ ]),
                          m4_eval(m4_len($1) - m4_index($1, [ ]))))])

#
# MF_LIB_CHECK(name,
#              lib&func list)
#
# This is a useful function that can be provided as a test to
# CHECK_EXTERNAL. It checks for a set of functions in a set of
# libraries to determine if the external is available.
#
# Arguments:
#  * name: The externals name 
#  * lib&func list: A list of library and function pairs that enumerate
#                   all required libraries for the external and a function
#                   to identify each,
#                    e.g. [[ssl SSL_connect], [crypto BF_encrypt]]
#
# Results:
# * cv_ext_<name> set to yes, if all libraries are found
#                        no, otherwise
#
# Note: This function uses _dir if it is set. It should be set to a
# path that includes a lib/ and include/ directory.
#
AC_DEFUN([MF_LIB_CHECK],
  [AS_IF([test "x$_dir" != x],
    [_ldflags_save="$LDFLAGS"
     _cflags_save="$CFLAGS"
     _ldflags="-L$_dir/lib"
     _cflags="-I$_dir/include"
     LDFLAGS="$LDFLAGS -L$_dir/lib"
     CFLAGS="$CFLAGS -I$_dir/include"])
     _libs=
     _failure=
     m4_foreach([ls], [$2],
      [AC_CHECK_LIB(mf_first(ls), mf_second(ls),
        [_libs="-l]mf_first(ls)[ $_libs"],
        [_failure="]mf_first(ls)[ $_failure"])])
     AS_IF([test "x$_failure" != x],
       [cv_ext_]m4_tolower($1)[=no
        LDFLAGS=$_ldflags_save
        CFLAGS=$_cflags_save
        AC_MSG_WARN([$1: could not find $_failure])],
        m4_if(x$3, x,
         [LIBS="$LIBS $_libs"],
         m4_toupper($3)[_LDFLAGS="$_ldflags $_libs"]
         m4_toupper($3)[_CFLAGS="$_cflags"]
         [LDFLAGS="$_ldflags_save"
          CFLAGS="$_cflags_save"])
        [cv_ext_]m4_tolower($1)[=yes])])

#
## THIS SHOULD WORK, BUT DOESN'T! FOR SOME REASON AC_CHECK_HEADER DOES
## NOT BEHAVE LIKE AC_CHECK_LIB
#
# MF_HEADER_CHECK(name,
#                 header)
#
# This is a useful function that can be provided as a test to
# CHECK_EXTERNAL. It checks for a header file to determine if the
# external is available.
#
# Arguments:
#  * name: The externals name 
#  * header: A header file to test for,
#             e.g. [classad/classad_distribution.h]
#
# Results:
# * cv_ext_<name> set to yes, if the header is found
#                        no, otherwise
#
# Note: This function uses _dir if it is set. It should be set to a
# path that includes a lib/ and include/ directory.
#
AC_DEFUN([MF_HEADER_CHECK],
  [_cflags_save="$CFLAGS"
   AS_IF([test "x$_dir" != x],
    [CFLAGS="$CFLAGS -I$_dir/include"])
   AC_CHECK_HEADER([$2],
     [cv_ext_]m4_tolower($1)[=yes],
     [cv_ext_]m4_tolower($1)[=no
      CFLAGS="$_cflags_save"
      AC_MSG_WARN([$1: could not find $2])])])

#######################################################################
# CONDOR_EXTERNAL_VERSION written by Derek Wright
# <wright@cs.wisc.edu> to specify the required version for a given
# external package needed for building Condor.  In addition to setting
# variables and printing out stuff, this macro checks to make sure
# that the externals directory tree we've been configured to use
# contains the given version of the external package.
# Arguments: 
#  $1 is the package "name" for the external
#  $2 is the package "version" of the external
# Side Effects:
#  If the given version is found in the externals tree, 
#  the variable $_cv_ext_<name>_version is set to hold the value, we
#  print out "checking <name> ... <version>, and we do a variable
#  substitution in our output files for "ext_<name>_version".
#
# Modifications by Matt, for use in CHECK_EXTERNAL when enable_proper
# is off:
#  * Sets cv_ext_<name> to yes or no if the external is available or not
#  * All exit points via a failure are changed to warnings, because
#    MF_EXTERNAL_CHECK will properly exit based on how required the
#    external is
#  * Side Effects no longer occur, they are handled by MF_EXTERNAL_CHECK
#######################################################################
AC_DEFUN([CONDOR_EXTERNAL_VERSION],
[CONDOR_EXTERNAL_VERSION_($1,$2,m4_toupper($1))])

AC_DEFUN([CONDOR_EXTERNAL_VERSION_],
[_bundle="$ac_cv_externals/bundles/$1"
 _bundle_version="$ac_cv_externals/bundles/$1/$2"
 _bundle_build="$ac_cv_externals/bundles/$1/$2/build_$1-$2"
 AC_MSG_CHECKING([for $1-$2])
 AS_IF([test -d "$_bundle" -a \
             -d "$_bundle_version" -a \
             -f "$_bundle_build"],
   [AC_MSG_RESULT([yes])
    cv_ext_$1=yes],
   [AC_MSG_RESULT([no])
    AC_MSG_WARN([$_bundle_build unavailable])
    cv_ext_$1=no])])


#######################################################################
# CONDOR_VERIFY_EXTERNALS_DIR written by Derek Wright
# <wright@cs.wisc.edu> to verify if a given directory is a valid
# externals directory tree needed for building Condor.  To simplify
# the macro and make the reporting consistent, this macro uses some
# helper macros (_condor_VERITY_*) to check for files and directories
# and report any errors found while trying to verify a given path.
# Arguments: 
#  $1 is the path to test
#  $2 is the error message to report if the given path is invalid
# Side Effects:
#  If the given path is valid, the variable $ac_cv_externals is set to
#  hold its value
#######################################################################
AC_DEFUN([CONDOR_VERIFY_EXTERNALS_DIR],
[AC_MSG_CHECKING([if $1 is valid])
 _condor_VERIFY_DIR([$1], [$2])
 _condor_VERIFY_FILE([$1/build_external], [$2])
 _condor_VERIFY_DIR([$1/bundles], [$2])
 AC_MSG_RESULT(yes)
 ac_cv_externals=$1
])


#######################################################################
# CONDOR_VERIFY_CONFIG_DIR written by Derek Wright
# <wright@cs.wisc.edu> to verify if a given directory is a valid
# config directory tree needed for building Condor.  To simplify
# the macro and make the reporting consistent, this macro uses some
# helper macros (_condor_VERITY_*) to check for files and directories
# and report any errors found while trying to verify a given path.
# Arguments: 
#  $1 is the path to test
#  $2 is the error message to report if the given path is invalid
# Side Effects:
#  If the given path is valid, the variable $ac_cv_config is set to
#  hold its value
#######################################################################
AC_DEFUN([CONDOR_VERIFY_CONFIG_DIR],
[AC_MSG_CHECKING([if $1 is valid])
 _condor_VERIFY_DIR([$1], [$2])
 _condor_VERIFY_FILE([$1/configure.cf.in], [$2])
 _condor_VERIFY_FILE([$1/externals.cf.in], [$2])
 _condor_VERIFY_FILE([$1/config.sh.in], [$2])
 AC_MSG_RESULT(yes)
 ac_cv_config=$1
])


#######################################################################
# _condor_VERIFY_FILE is a helper for the various _condor_VERIFY_*
# macros 
# Arguments:
#  $1 file to test
#  $2 is the general error message to print which was given to
#     _condor_VERIFY_*
# Side Effects:
#   If the given file doesn't exist, this macro calls
#   _condor_VERIFY_ERROR which will terminate with AC_MSG_ERROR
#######################################################################
AC_DEFUN([_condor_VERIFY_FILE],
[if test ! -f "$1"; then
   _condor_VERIFY_ERROR([$1 does not exist], [$2])
 fi
])


#######################################################################
# _condor_VERIFY_DIR is a helper for the _condor_VERIFY_* macros 
# Arguments:
#  $1 directory to test
#  $2 is the general error message to print which was given to
#     _condor_VERIFY_*
# Side Effects:
#   If the given directory doesn't exist, this macro calls
#   _condor_VERIFY_ERROR which will terminate with AC_MSG_ERROR
#######################################################################
AC_DEFUN([_condor_VERIFY_DIR],
[if test ! -d "$1"; then
   _condor_VERIFY_ERROR([$1 is not a directory], [$2])
 fi
])


#######################################################################
# _condor_VERIFY_ERROR is a helper for the _condor_VERIFY_* macros
# Arguments:
#  $1 is the specific warning message about why a directory is invalid
#  $2 is the general error message to print which was given to
#     _condor_VERIFY_*_DIR
# Side Effects:
#  Calling this macro terminates configure with AC_MSG_ERROR
#######################################################################
AC_DEFUN([_condor_VERIFY_ERROR],
[AC_MSG_RESULT([no])
 AC_MSG_WARN([$1])
 AC_MSG_ERROR([$2])
])


#######################################################################
# GET_GCC_VALUE by Derek Wright <wright@cs.wisc.edu> 
# We need to find out a few things from our gcc installation, like the
# full paths to some of the support libraries, etc.  This macro just
# invokes gcc with a given option, saves the value into a variable, and
# if it worked, it substitutes the discovered value.
# Arguments: 
#  $1 the "checking for" message to print
#  $2 the gcc option to envoke
#  $3 the variable name (used for storing in cache with "_cv_"
#     prepended to the front, and for substitution in the output)
#######################################################################
AC_DEFUN([GET_GCC_VALUE],
[AC_CACHE_CHECK( [for $1], [_cv_[$3]],
 [[_cv_$3]=`gcc $2 2>/dev/null`
  _gcc_status=$?
  if test $_gcc_status -ne 0; then
    [_cv_$3]="no";
  fi
 ])
 if test $[_cv_[$3]] = "no"; then
   AC_MSG_RESULT([no])
 else
   AC_MSG_RESULT([yes])
   AC_SUBST($3,$[_cv_[$3]])
 fi
])

#######################################################################
# C_CXX_OPTION by Peter Keller <psilord@cs.wisc.edu>
# We need to see whether or not a particular flag is available for a
# compiler (which should be either a C or C++ compiler).
#  $1 the compiler path to check
#  $2 the warning option to check
#  $3 the variable name (used for storing in cache with "_cv_"
#     prepended to the front, and for substitution in the output)
#  $4 any optional flags which should be passed to gcc to test the flag in
#     question to test the first flag
#######################################################################
AC_DEFUN([C_CXX_OPTION],
[AC_CACHE_CHECK( [if $1 supports $2], [_cv_$3],
cat > conf_flag.c << EOF
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
	return 0;
}
EOF
 # Stupid compilers sometimes say "Invalid option" or "unrecognized option",
 # but they always seem to say "option" in there.... Also sometimes an
 # executable is produced, other times not. So we'll assume that if the word
 # "option" shows up at all in the stderr of the compiler, it is unrecognized.
 [[_cv_$3]=`$1 $2 $4 conf_flag.c -c -o conf_flag.o 2>&1 | grep "option" > /dev/null 2>&1`
  _comp_status=$?
  # the test seems inverted because the failure of grep to find the 
  # sentinel value means gcc accepted it
  if test $_comp_status -eq 1; then
    [_cv_$3]="yes";
  else
    [_cv_$3]="no";
  fi
 ])
 if test $[_cv_$3] = "yes"; then
   AC_SUBST($3,$2)
 fi
  rm -f conf_flag.c conf_flag.o
])

#######################################################################
# CONDOR_TRY_CP_RECURSIVE_SYMLINK_FLAG by <wright@cs.wisc.edu> 
# When packaging Condor, we sometimes need to recursively copy files.
# Hoewver, when we do, we want to dereference symlinks and copy what
# they point to, not copy the links themselves.  Unfortunately, the
# default behavior with GNU cp using "-r" is to copy the links, not 
# dereference them.  So, we check in the configure script what option
# works to both copy recursively and dereference symlinks.  This macro
# is used inside that loop to do the actual testing.  
# Arguments: 
#  $1 the full path to cp to try
#  $2 the command-line option to cp to test
#  $3 the variable name to set to "none" if the flag failed, or to
#     hold the flag itself if it worked.
#######################################################################
AC_DEFUN([CONDOR_TRY_CP_RECURSIVE_SYMLINK_FLAG],
[
 # initialize the flag imagining we're going to fail.  we'll set it to
 # the right thing if the flag really worked
 [$3]="none";
 # beware, at this stage, all we can count on is the configure script
#itself ex
 touch conftest_base > /dev/null 2>&1
 ln -s conftest_base conftest_link > /dev/null 2>&1
 $1 $2 conftest_link conftest_file > /dev/null 2>&1
 if test -f "conftest_file" ; then
   perl -e 'exit -l "conftest_file"' > /dev/null 2>&1
   _is_link=$?
   if test "$_is_link" = "0" ; then
   # it's not a symlink!  this flag will work.
     [$3]="$2";
   fi
 fi
 # either way, clean up our test files
 /bin/rm -f conftest_base conftest_link conftest_file > /dev/null 2>&1
])


#######################################################################
# CONDOR_TEST_STRIP by Derek Wright <wright@cs.wisc.edu> 
# We need to use strip for packaging Condor.  Usually, we prefer GNU
# strip but sometimes we need to use the vendor strip.  So, this macro
# is used for testing a given version of strip to see if it produces
# an output program that still works, and if so, how big the resulting
# output program is in bytes.  We use this macro on both GNU and
# vendor strip (if we find them), and if both versions work, we use
# the size to determine which version to use.
# 
# Unfortunately, this test has to be fairly unorthodox, since it
# doesn't neatly fall into the standard way of testing things.  First
# of all, we can't use autoconf's compiler or link tests, since we
# need to run strip against the output, and all of autoconf's pre-made
# tests clean up after themselves.  Also, we can't really use the
# cache for this, since we want to check 2 values, and we can't do
# that without breaking some cache rules (like no side effects in your
# cache check).  Also, we assume "g++" is in the PATH and works (we
# want to make a c++ test program, since that's what we're going to be
# stripping, anyway).  Therefore, this macro should be called *AFTER*
# we test for g++ 
#
# Arguments
#  $1 identifying message to print 
#  $2 variable name of the strip to test.  not only is this used to
#     invoke strip, it's also used for naming a few other variables
# Side Effects:
#  Sets a few variables with the result of the test:
#    "<variable_name>_works"=(yes|no)  
#    "<variable_name>_outsize"=(size in bytes of the stripped program)
#######################################################################
AC_DEFUN([CONDOR_TEST_STRIP],
[AC_MSG_CHECKING([if $1 works])
 [$2]_works="no"
 [$2]_outsize=""
 cat > conftest.c << _TESTEOF
#include <stdio.h>
int main () {
  printf( "hello world\n" );
  return 0;
}
_TESTEOF
 g++ -g -o conftest1 conftest.c > /dev/null 2>&1
 if test $? -ne 0 ; then
   AC_MSG_RESULT([no])
   AC_MSG_ERROR([failed to build simple gcc test program])
 fi
 $[$2] conftest1 > /dev/null 2>&1
 if test $? -eq 0 &&
      (sh -c ./conftest1 2>/dev/null |grep "hello world" 2>&1 >/dev/null) ;
 then
   [$2]_works="yes"
   [$2]_outsize="`/bin/ls -nl conftest1 2> /dev/null | awk '{print [$]5}'`"
   AC_MSG_RESULT([yes])
 else
   AC_MSG_RESULT([no])
 fi
 rm -f conftest.c conftest1
])


#######################################################################
# REQUIRE_PATH_PROG by Derek Wright <wright@cs.wisc.edu> 
# Performs an AC_PATH_PROG, and then makes sure that we found the
# program we were looking for.
#
# Arguments (just the first two you pass to AC_PATH_PROG)
#  $1 variable name 
#  $2 program name
# Side Effects:
#  Calls AC_PATH_PROG, which does a bunch of stuff for you.
#######################################################################
AC_DEFUN([REQUIRE_PATH_PROG],
[AC_PATH_PROG([$1],[$2],[no],[$PATH])
 if test "$[$1]" = "no" ; then
   AC_MSG_ERROR([$2 is required])
 fi
])


#######################################################################
# CHECK_PATH_PROG by Derek Wright <wright@cs.wisc.edu> 
# Checks for the full path to the requested tool.
# Assumes you already called AC_PATH_PROG(WHICH,which).
# We can't use AC_REQUIRE for that assumption, b/c that only works
# for macros without arguments.
#
# Arguments
#  $1 program name
#  $2 variable name prefix 
# Side Effects:
#  sets $2_path to the full path to $1
#######################################################################
AC_DEFUN([CHECK_PATH_PROG],
[AC_MSG_CHECKING([for full path to $1])
 if test $WHICH != no; then
   $2_path=`$WHICH $1`
   AC_MSG_RESULT($[$2_path])
 else
   AC_MSG_RESULT([this platform does not support 'which'])
 fi
])

dnl - I found this on the internet. It was from gdal, which is MIT-licensed.
dnl The license on the macro is listed as "AllPermissive" which according to
dnl http://ac-archive.sourceforge.net/doc/contribute.html
dnl means "assumed MIT-style"
dnl
dnl
dnl @synopsis AX_LIB_ORACLE_OCI([MINIMUM-VERSION])
dnl
dnl This macro provides tests of availability of Oracle OCI API
dnl of particular version or newer.
dnl This macros checks for Oracle OCI headers and libraries 
dnl and defines compilation flags
dnl 
dnl Macro supports following options and their values:
dnl 1) Single-option usage:
dnl --with-oci - path to ORACLE_HOME directory
dnl 2) Two-options usage (both options are required):
dnl --with-oci-include - path to directory with OCI headers
dnl --with-oci-lib - path to directory with OCI libraries 
dnl
dnl NOTE: These options described above does not take yes|no values.
dnl If 'yes' value is passed, then WARNING message will be displayed,
dnl 'no' value, as well as the --without-oci-* variations will cause
dnl the macro won't check enything.
dnl
dnl This macro calls:
dnl
dnl   AC_SUBST(ORACLE_OCI_CFLAGS)
dnl   AC_SUBST(ORACLE_OCI_LDFLAGS)
dnl   AC_SUBST(ORACLE_OCI_VERSION)
dnl
dnl And sets:
dnl
dnl   HAVE_ORACLE_OCI
dnl
dnl @category InstalledPackages
dnl @category Cxx
dnl @author Mateusz Loskot <mateusz@loskot.net>
dnl @license AllPermissive
dnl
AC_DEFUN([AX_LIB_ORACLE_OCI],
[
    AC_ARG_WITH([oci],
        AC_HELP_STRING([--with-oci=@<:@ARG@:>@],
            [use Oracle OCI API from given Oracle home (ARG=path); use existing ORACLE_HOME (ARG=yes); disable Oracle OCI support (ARG=no)]
        ),
        [
        if test "$withval" = "yes"; then
            if test -n "$ORACLE_HOME"; then
                oracle_home_dir="$ORACLE_HOME"
            else
                oracle_home_dir=""
            fi 
        elif test -d "$withval"; then
            oracle_home_dir="$withval"
        else
            oracle_home_dir=""
        fi
        ],
        [
        if test -n "$ORACLE_HOME"; then
            oracle_home_dir="$ORACLE_HOME"
        else
            oracle_home_dir=""
        fi 
        ]
    )

    AC_ARG_WITH([oci-include],
        AC_HELP_STRING([--with-oci-include=@<:@DIR@:>@],
            [use Oracle OCI API headers from given path]
        ),
        [oracle_home_include_dir="$withval"],
        [oracle_home_include_dir=""]
    )
    AC_ARG_WITH([oci-lib],
        AC_HELP_STRING([--with-oci-lib=@<:@DIR@:>@],
            [use Oracle OCI API libraries from given path]
        ),
        [oracle_home_lib_dir="$withval"],
        [oracle_home_lib_dir=""]
    )

    ORACLE_OCI_CFLAGS=""
    ORACLE_OCI_LDFLAGS=""
    ORACLE_OCI_VERSION=""

    dnl
    dnl Collect include/lib paths
    dnl 
    want_oracle_but_no_path="no"

    if test -n "$oracle_home_dir"; then

        if test "$oracle_home_dir" != "no" -a "$oracle_home_dir" != "yes"; then
            dnl ORACLE_HOME path provided
            oracle_include_dir="$oracle_home_dir/rdbms/public"
            oracle_lib_dir="$oracle_home_dir/lib"
        elif test "$oracle_home_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi

    elif test -n "$oracle_home_include_dir" -o -n "$oracle_home_lib_dir"; then

        if test "$oracle_home_include_dir" != "no" -a "$oracle_home_include_dir" != "yes"; then
            oracle_include_dir="$oracle_home_include_dir"
        elif test "$oracle_home_include_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi

        if test "$oracle_home_lib_dir" != "no" -a "$oracle_home_lib_dir" != "yes"; then
            oracle_lib_dir="$oracle_home_lib_dir"
        elif test "$oracle_home_lib_dir" = "yes"; then
            want_oracle_but_no_path="yes"
        fi
    fi

    if test "$want_oracle_but_no_path" = "yes"; then
        AC_MSG_WARN([Oracle support is requested but no Oracle paths have been provided. \
Please, locate Oracle directories using --with-oci or \
--with-oci-include and --with-oci-lib options.])
    fi

    dnl
    dnl Check OCI files
    dnl
    if test -n "$oracle_include_dir" -a -n "$oracle_lib_dir"; then

        saved_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS -I$oracle_include_dir"

        dnl Depending on later Oracle version detection,
        dnl -lnnz10 flag might be removed for older Oracle < 10.x
        saved_LDFLAGS="$LDFLAGS"
        oci_ldflags="-L$oracle_lib_dir -locci -lclntsh"
        LDFLAGS="$LDFLAGS $oci_ldflags -lnnz10"

        dnl
        dnl Check OCI headers
        dnl
        AC_MSG_CHECKING([for Oracle OCI headers in $oracle_include_dir])

        AC_LANG_PUSH(C++)
        AC_COMPILE_IFELSE([
            AC_LANG_PROGRAM([[@%:@include <oci.h>]],
                [[
#if defined(OCI_MAJOR_VERSION)
#if OCI_MAJOR_VERSION == 10 && OCI_MINOR_VERSION == 2
// Oracle 10.2 detected
#endif
#elif defined(OCI_V7_SYNTAX)
// OK, older Oracle detected
// TODO - mloskot: find better macro to check for older versions; 
#else
#  error Oracle oci.h header not found
#endif
                ]]
            )],
            [
            ORACLE_OCI_CFLAGS="-I$oracle_include_dir"
            oci_header_found="yes"
            AC_MSG_RESULT([yes])
            ],
            [
            oci_header_found="no"
            AC_MSG_RESULT([not found])
            ]
        )
        AC_LANG_POP([C++])
        
        dnl
        dnl Check OCI libraries
        dnl
        if test "$oci_header_found" = "yes"; then

            AC_MSG_CHECKING([for Oracle OCI libraries in $oracle_lib_dir])

            AC_LANG_PUSH(C++)
            AC_LINK_IFELSE([
                AC_LANG_PROGRAM([[@%:@include <oci.h>]],
                    [[
OCIEnv* envh = 0;
OCIEnvCreate(&envh, OCI_DEFAULT, 0, 0, 0, 0, 0, 0);
if (envh) OCIHandleFree(envh, OCI_HTYPE_ENV);
                    ]]
                )],
                [
                ORACLE_OCI_LDFLAGS="$oci_ldflags"
                oci_lib_found="yes"
                AC_MSG_RESULT([yes])
                ],
                [
                oci_lib_found="no"
                AC_MSG_RESULT([not found])
                ]
            )
            AC_LANG_POP([C++])
        fi

        CPPFLAGS="$saved_CPPFLAGS"
        LDFLAGS="$saved_LDFLAGS"
    fi

    dnl
    dnl Check required version of Oracle is available
    dnl
    oracle_version_req=ifelse([$1], [], [], [$1])

    if test "$oci_header_found" = "yes" -a "$oci_lib_found" = "yes" -a \
        -n "$oracle_version_req"; then

        oracle_version_major=`cat $oracle_include_dir/oci.h \
                             | grep '#define.*OCI_MAJOR_VERSION.*' \
                             | sed -e 's/#define OCI_MAJOR_VERSION  *//' \
                             | sed -e 's/  *\/\*.*\*\///'`

        oracle_version_minor=`cat $oracle_include_dir/oci.h \
                             | grep '#define.*OCI_MINOR_VERSION.*' \
                             | sed -e 's/#define OCI_MINOR_VERSION  *//' \
                             | sed -e 's/  *\/\*.*\*\///'`

        AC_MSG_CHECKING([if Oracle OCI version is >= $oracle_version_req])

        if test -n "$oracle_version_major" -a -n $"oracle_version_minor"; then

            ORACLE_OCI_VERSION="$oracle_version_major.$oracle_version_minor"

            dnl Decompose required version string of Oracle
            dnl and calculate its number representation
            oracle_version_req_major=`expr $oracle_version_req : '\([[0-9]]*\)'`
            oracle_version_req_minor=`expr $oracle_version_req : '[[0-9]]*\.\([[0-9]]*\)'`

            oracle_version_req_number=`expr $oracle_version_req_major \* 1000000 \
                                       \+ $oracle_version_req_minor \* 1000`

            dnl Calculate its number representation 
            oracle_version_number=`expr $oracle_version_major \* 1000000 \
                                  \+ $oracle_version_minor \* 1000`

            oracle_version_check=`expr $oracle_version_number \>\= $oracle_version_req_number`
            if test "$oracle_version_check" = "1"; then

                oracle_version_checked="yes"
                AC_MSG_RESULT([yes])

                dnl Add -lnnz10 flag to Oracle >= 10.x
                AC_MSG_CHECKING([for Oracle version >= 10.x to use -lnnz10 flag])
                oracle_nnz10_check=`expr $oracle_version_number \>\= 10 \* 1000000`
                if test "$oracle_nnz10_check" = "1"; then
                    ORACLE_OCI_LDFLAGS="$ORACLE_OCI_LDFLAGS -lnnz10"
                    AC_MSG_RESULT([yes])
                else
                    AC_MSG_RESULT([no])
                fi
            else
                oracle_version_checked="no"
                AC_MSG_RESULT([no])
                AC_MSG_ERROR([Oracle $ORACLE_OCI_VERSION found, but required version is $oracle_version_req])
            fi
        else
            ORACLE_OCI_VERSION="UNKNOWN"
            AC_MSG_RESULT([no])
            AC_MSG_WARN([Oracle version unknown, probably OCI older than 10.2 is available])
        fi
    fi

    AC_MSG_CHECKING([if Oracle support is enabled])

    if test "$oci_header_found" = "yes" -a "$oci_lib_found" = "yes"; then

        AC_SUBST([ORACLE_OCI_VERSION])
        AC_SUBST([ORACLE_OCI_CFLAGS])
        AC_SUBST([ORACLE_OCI_LDFLAGS])

        HAVE_ORACLE_OCI="yes"
    else
        HAVE_ORACLE_OCI="no"
    fi
    
    AC_MSG_RESULT([$HAVE_ORACLE_OCI])
])

##### http://autoconf-archive.cryp.to/ac_func_snprintf.html
#
# SYNOPSIS
#
#   AC_FUNC_SNPRINTF
#
# DESCRIPTION
#
#   Checks for a fully C99 compliant snprintf, in particular checks
#   whether it does bounds checking and returns the correct string
#   length; does the same check for vsnprintf. If no working snprintf
#   or vsnprintf is found, request a replacement and warn the user
#   about it. Note: the mentioned replacement is freely available and
#   may be used in any project regardless of it's license.
#
# LAST MODIFICATION
#
#   2006-10-18
#
# COPYLEFT
#
#   Copyright (c) 2006 Rüdiger Kuhlmann <info@ruediger-kuhlmann.de>
#
#   Copying and distribution of this file, with or without
#   modification, are permitted in any medium without royalty provided
#   the copyright notice and this notice are preserved.

AC_DEFUN([AC_FUNC_SNPRINTF],
[AC_CHECK_FUNCS(snprintf vsnprintf)
AC_MSG_CHECKING(for working snprintf)
AC_CACHE_VAL(ac_cv_have_working_snprintf,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = snprintf (bufs, 2, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = snprintf (bufd, 2, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_snprintf=yes, ac_cv_have_working_snprintf=no, ac_cv_have_working_snprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_snprintf])
AC_MSG_CHECKING(for working vsnprintf)
AC_CACHE_VAL(ac_cv_have_working_vsnprintf,
[AC_TRY_RUN(
[#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

int my_vsnprintf (char *buf, const char *tmpl, ...)
{
    int i;
    va_list args;
    va_start (args, tmpl);
    i = vsnprintf (buf, 2, tmpl, args);
    va_end (args);
    return i;
}

int main(void)
{
    char bufs[5] = { 'x', 'x', 'x', '\0', '\0' };
    char bufd[5] = { 'x', 'x', 'x', '\0', '\0' };
    int i;
    i = my_vsnprintf (bufs, "%s", "111");
    if (strcmp (bufs, "1")) exit (1);
    if (i != 3) exit (1);
    i = my_vsnprintf (bufd, "%d", 111);
    if (strcmp (bufd, "1")) exit (1);
    if (i != 3) exit (1);
    exit(0);
}], ac_cv_have_working_vsnprintf=yes, ac_cv_have_working_vsnprintf=no, ac_cv_have_working_vsnprintf=cross)])
AC_MSG_RESULT([$ac_cv_have_working_vsnprintf])
if test x$ac_cv_have_working_snprintf$ac_cv_have_working_vsnprintf != "xyesyes"; then
  AC_MSG_WARN([Replacing missing/broken (v)snprintf() with our own version.])
 else
  AC_DEFINE(HAVE_WORKING_SNPRINTF, 1, "use system (v)snprintf instead of our replacement")
fi])

##################################
# Test for Thread Local Storage.
# If found, define HAVE_TLS and also set TLS to be the storage class keyword.
# Currently this function tries keywords __thread (GNU) and MS VC++ keywords.
#################################
AC_DEFUN([AX_TLS], [
  AC_MSG_CHECKING(for thread local storage (TLS) class)
  AC_CACHE_VAL(ac_cv_tls, [
    ax_tls_keywords="__thread __declspec(thread) none"
    for ax_tls_keyword in $ax_tls_keywords; do
       case $ax_tls_keyword in
          none) ac_cv_tls=none ; break ;;
          *)
             AC_TRY_COMPILE(
                [#include <stdlib.h>
                 static void
                 foo(void) {
                 static ] $ax_tls_keyword [ int bar;
                 exit(1);
                 }],
                 [],
                 [ac_cv_tls=$ax_tls_keyword ; break],
                 ac_cv_tls=none
             )
          esac
    done
])

  if test "$ac_cv_tls" != "none"; then
    dnl AC_DEFINE([TLS], [], [If the compiler supports a TLS storage class define it to that here])
    AC_DEFINE_UNQUOTED([TLS], $ac_cv_tls, [If the compiler supports a TLS storage class define it to that here])
    AC_DEFINE([HAVE_TLS],[1],[Do we have support for Thread Local Storage?])
  fi
  AC_MSG_RESULT($ac_cv_tls)
])

