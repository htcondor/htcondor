#######################################################################
# CHECK_PROG_IS_GNU is based loosely on the CHECK_GNU_MAKE macro 
# available from the GNU Autoconf Macro Archive at:
# http://www.gnu.org/software/ac-archive/htmldoc/check_gnu_make.html
# This version is more generic in that it checks any given program if
# it's GNU or not.  However, it's more specific that it assumes the
# given program is required.  If it doesn't find it, it bails out with
# a fatal error.  Written by Derek Wright <wright@cs.wisc.edu>
# Arguments:
#  $1 the variable you want to store the program in (if found) 
#  $2 is the name of the program to search for
# Side effects:
#  Bails out if the program isn't found.
#  Sets _cv_<progname>_is_gnu to "yes" or "no" as appropriate
#######################################################################
AC_DEFUN( [CHECK_PROG_IS_GNU],
[AC_CHECK_PROG($1,$2,$2)
 if test "$[$1]" != "$2"; then
   AC_MSG_ERROR( [$2 is required] )
 fi
 AC_CACHE_CHECK( [if $2 is GNU], [_cv_$2_is_gnu],
 [if ( sh -c "$[$1] --version" 2> /dev/null | grep GNU > /dev/null 2>&1 ) ;
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
  AC_CHECK_PROG(PERL,perl,perl)
fi
# Check if version of Perl is sufficient
ac_perl_version="$1"
if test "x$PERL" != "x"; then
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


#######################################################################
# CONDOR_VERIFY_EXTERNALS_DIR written by Derek Wright
# <wright@cs.wisc.edu> to verify if a given directory is a valid
# externals directory tree needed for building Condor.  To simplify
# the macro and make the reporting consistent, this macro uses a
# simple helper macro (_condor_VERITY_EXT_ERROR) to report any errors
# found while trying to verify a given path.
# Arguments: 
#  $1 is the path to test
#  $2 is the error message to report if the given path is invalid
# Side Effects:
#  If the given path is valid, the variable $ac_cv_externals is set to
#  hold its value
#######################################################################
AC_DEFUN([CONDOR_VERIFY_EXTERNALS_DIR],
[AC_MSG_CHECKING([if $1 is valid])
 if test ! -d $1; then
   _condor_VERIFY_EXT_ERROR([$1 is not a directory], [$2])
 fi  
 if test ! -f "$1/build_external"; then
   _condor_VERIFY_EXT_ERROR([$1/build_external script does not exist], [$2])
 fi
 if test ! -d "$1/bundles"; then
   _condor_VERIFY_EXT_ERROR([$1/bundles is not a directory], [$2])
 fi
 AC_MSG_RESULT(yes)
 ac_cv_externals=$1
])

#######################################################################
# CONDOR_VERIFY_EXT_ERROR is a helper for CONDOR_VERIFY_EXTERNALS_DIR 
# Arguments:
#  $1 is the specific warning message about why a directory is invalid
#  $2 is the general error message to print which was given to
#     _condor_VERIFY_EXTERNALS_DIR
# Side Effects:
#  Calling this macro terminates configure with AC_MSG_ERROR() 
#######################################################################
AC_DEFUN([_condor_VERIFY_EXT_ERROR],
[AC_MSG_RESULT([no])
 AC_MSG_WARN([$1])
 AC_MSG_ERROR([$2])
])


#######################################################################
# CONDOR_VERIFY_ANALYZE_DIR by Derek Wright <wright@cs.wisc.edu>
# Verifies if a given directory has valid condor_analyze binaries for
# the Condor release.
# Arguments: 
#  $1 is the path to test
# Side Effects:
#  If the given path is valid, the variable $ac_condor_analyze_dir is
#  set to hold its value, and $ac_has_condor_analyze is set to "YES".
#  Additionally, if the directory contains any condor_analyze binaries
#  (ending in ".static", ".strip" and/or ".release",
#  $ac_condor_analyze_(static|strip|release) are set to the full
#  paths of the corresponding binary.
#######################################################################
AC_DEFUN([CONDOR_VERIFY_ANALYZE_DIR],
[
 ac_has_condor_analyze=NO
 if test -d $1 ; then
   if test -f "$1/condor_analyze.strip" ; then
     ac_condor_analyze_strip="$1/condor_analyze.strip"
     ac_has_condor_analyze=YES
   fi
   if test -f "$1/condor_analyze.static" ; then
     ac_condor_analyze_static="$1/condor_analyze.static"
     ac_has_condor_analyze=YES
   fi
   if test -f "$1/condor_analyze.release" ; then
     ac_condor_analyze_release="$1/condor_analyze.release"
     ac_has_condor_analyze=YES
   fi
   if test -f "$1/condor_analyze" ; then
     ac_condor_analyze="$1/condor_analyze"
     ac_has_condor_analyze=YES
   fi
   if test $ac_has_condor_analyze = "YES" ; then
     ac_condor_analyze_dir=$1
   fi
 fi
])


#######################################################################
# CONDOR_SET_ANALYZE by Derek Wright <wright@cs.wisc.edu> 
# Once we know what binaries we're using for Condor analyze, this
# macro is used as a helper to keep configure.ac more readable.  It is
# responsible for calling the right AC_SUBST() macro, depending on if
# we found the per-type version (release, static, or strip), or just a
# plain "condor_analyze" binary.
# Arguments: 
#  $1 is the per-type shell variable to test
#  $2 is the keyword used in the output variable to set and filenames
#  $3 a string, either "fatal" or not, to specify if this macro should
#     call AC_MSG_ERROR() if neither the per-type nor generic version
#     of condor_analyze was found
#######################################################################
AC_DEFUN([CONDOR_SET_ANALYZE],
[
 if test "x$[$1]" = "x"; then
   if test ! "x$ac_condor_analyze" = "x" ; then
     AC_MSG_WARN([no condor_analyze.$2 found, using $ac_condor_analyze])
     AC_SUBST([condor_analyze_[$2]],$ac_condor_analyze)
   else
     if test "$3" = "fatal" ; then 
       AC_MSG_ERROR([neither condor_analyze.$2 nor condor_analyze found])
     else
       AC_MSG_WARN([neither condor_analyze.$2 nor condor_analyze found])
     fi
   fi
 else 
   AC_SUBST([condor_analyze_[$2]],$[$1])
 fi
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
   AC_MSG_ERROR([Can not find $1: gcc $2 failed!])
 else
   AC_SUBST($3,$[_cv_[$3]])
 fi
])
