# Configure template for Condor.                             -*-Autoconf-*- 
# Process this file with autoconf to produce a configure script.
############################Copyright-DO-NOT-REMOVE-THIS-LINE##
#
# Condor Software Copyright Notice
# Copyright (C) 1990-2004, Condor Team, Computer Sciences Department,
# University of Wisconsin-Madison, WI.
#
# This source code is covered by the Condor Public License, which can
# be found in the accompanying LICENSE.TXT file, or online at
# www.condorproject.org.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# AND THE UNIVERSITY OF WISCONSIN-MADISON "AS IS" AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, OF SATISFACTORY QUALITY, AND FITNESS
# FOR A PARTICULAR PURPOSE OR USE ARE DISCLAIMED. THE COPYRIGHT
# HOLDERS AND CONTRIBUTORS AND THE UNIVERSITY OF WISCONSIN-MADISON
# MAKE NO MAKE NO REPRESENTATION THAT THE SOFTWARE, MODIFICATIONS,
# ENHANCEMENTS OR DERIVATIVE WORKS THEREOF, WILL NOT INFRINGE ANY
# PATENT, COPYRIGHT, TRADEMARK, TRADE SECRET OR OTHER PROPRIETARY
# RIGHT.
#
############################Copyright-DO-NOT-REMOVE-THIS-LINE##

# We need a recent autoconf for many of the macros and tests we're using
AC_PREREQ(2.57)

# Initialize autoconf
AC_INIT


# first, if we notice we're being run at UW-Madison CS on a
# CSL-supported machine, and the PATH doesn't include
# /p/condor/workspaces/build/bin before /s/std/bin, print an error
# message and exit, since there's no way we're going to find the first
# versions of everything we need, and it's easier to just tell the
# user right at the start what they need to do to fix it, instead of
# having the rest of the tests in here fail...
my_domain=`echo $ac_hostname | cut -f2- -d.`
_bad_path_dirs="/s/std/bin /usr/local/bin /usr/bin /bin"
_found_bad_dir=no
_which_bad_dir=""
_found_s_std_bin=no
_found_p_build_bin=no
_p_build_bin="/p/condor/workspaces/build/bin"
if test $my_domain = "cs.wisc.edu" && test -d "/s/std/bin" ; then
  AC_MSG_CHECKING([if PATH on CSL-supported machine is ok]) 
  as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
  for as_dir in $PATH
  do
    IFS=$as_save_IFS
    for test_dir in $_bad_path_dirs
    do
      if test $as_dir = $test_dir ; then
        _found_bad_dir=yes
	if test "x$_which_bad_dir" = "x" ; then 
          _which_bad_dir="$test_dir"
	else
          _which_bad_dir="$_which_bad_dir, $test_dir"
	fi  
      fi
    done  
    if test $as_dir = $_p_build_bin ; then
      if test $_found_bad_dir = "yes" ; then
        AC_MSG_RESULT([no]) 
        AC_MSG_ERROR([You are compiling on a UW-Madison CSL-supported machine and $_p_build_bin is *NOT* near the front of your PATH.  This configure script will not be able to find the right versions of the programs it needs to build Condor.  Please fix your PATH by putting $_p_build_bin *BEFORE* any of these directories in your PATH: $_which_bad_dir])
      fi
      _found_p_build_bin=yes
      break
    fi
  done
  if test $_found_p_build_bin = "no" ; then
    AC_MSG_RESULT([no]) 
    AC_MSG_ERROR([You are compiling on a UW-Madison CSL-supported machine and $_p_build_bin is *NOT* in your PATH.  This configure script will not be able to find the right versions of programs it needs to build Condor.  Please fix your PATH by adding $_p_build_bin (it must be before any of these directories: $_bad_path_dirs) and re-run configure])
  fi
  AC_MSG_RESULT([yes])
fi   

# Condor only requires 5.002, but GPT apparently relies on 5.005_03 or
# greater.  Unfortunately, GPT doesn't have a configure script or
# anything, it just lets perl die with a require statement.  So, we
# test for the right version here and fail in an early and obvious way
# if there's a problem.  Also, we check for this early, so we can use
# perl in the rest of the tests in here, if we want it. 
AC_PROG_PERL_VERSION([5.005_03], [],
   [AC_MSG_ERROR([perl version 5.005_03 or higher is required])])


############################################################
# Figure out what platform we're being run on
############################################################
AC_MSG_NOTICE([determining host platform])
arch=`uname -m`
osrelease=`uname -r`
osversion=`uname -v`
_cv_arch=""
_cv_opsys=""
_cv_opsys_vers=""
_cv_sysname=""
_cv_linux_distro="UNKNOWN"
_cv_libc_kind="LIBC-UNKNOWN"
_cv_libc_major="0"
_cv_libc_minor="0"
_cv_libc_patch="0"

# Make sure if we get back something that begins with "IRIX" that we
# strip off anything after it (e.g. "IRIX64", etc).
os=`uname -s | sed 's/IRIX.*/IRIX/'`

if test "$os" = "HP-UX"; then
  _cv_opsys="HPUX"
  _cv_arch="HPPA"
  if test "$osrelease" = "B.10.20" ; then
    _cv_opsys_vers="HPUX10"
    _cv_sysname="hp_ux102"
  elif test "$osversion" = "B.11.00" ; then
    _cv_opsys_vers="HPUX11"
    _cv_sysname="hp_ux110"
  fi

elif test "$os" = "OSF1" ; then
  _cv_arch="ALPHA"
  _cv_opsys="DUX"
  if test $osrelease = "V4.0" ; then
    _cv_opsys_vers="DUX4"
    _cv_sysname="alpha_dux40"
  elif test $osrelease = "V5.0" -o $osrelease = "V5.1" ; then
    _cv_opsys_vers="DUX5"
    _cv_sysname="alpha_dux51"
  fi

elif test $os = "Linux" ; then
  # linux is a big mess, since we have different distributions,
  # different versions of glibc, etc, etc...
  _cv_opsys="LINUX"
  if test $arch = "i686" -o $arch = "i586" -o $arch = "i486" ; then
    _cv_arch="I386"
  elif test $arch = "alpha" ; then
    _cv_arch="ALPHA"
  elif test $arch = "ia64" ; then
    _cv_arch="IA64"
  fi

  # next, figure out what linux distribution this is
  if test -f "/etc/redhat-release" ; then 
    _rh_vers=`cat /etc/redhat-release|perl -ne '/.* (.*) \(.*\)/;print $1;'`
    case "$_rh_vers" in
     "7.2" )
       _cv_linux_distro="RH72"
       ;;
     "2.1AW" )
       _cv_linux_distro="RH72"
       ;;
     "8.0" )
       _cv_linux_distro="RH80"
       ;;
     "9" )
       _cv_linux_distro="RH9"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to redhat version $_rh_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
       _cv_linux_distro="UNKNOWN"
       ;;
    esac
  elif test -f "/etc/SuSE-release" ; then
    _suse_vers=`cat /etc/SuSE-release | grep VERSION | awk '{print $3}'`
    case "$_suse_vers" in
     "8.1" )
       _cv_linux_distro="SLES81"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to SuSE version $_suse_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
       _cv_linux_distro="UNKNOWN"
       ;;
    esac
  else
    AC_MSG_WARN([Unrecognised Linux distribution])
    AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
    _cv_linux_distro="UNKNOWN"
  fi
  # this perl 1-liner takes all space-delimited input, sticks it
  # together with '_' marks, and lower-cases the whole thing
  _cv_sysname="`echo $_cv_arch $_cv_linux_distro | perl -ane 'print lc join('_',@F)'`"
  _cv_opsys_vers="LINUX-$_cv_linux_distro"

elif test $os = "SunOS" ; then
  _cv_opsys="SOLARIS"
  if test $arch = "sun4m" -o $arch = "sun4u" ; then
    _cv_arch="SUN4X"
  else
    AC_MSG_ERROR([Condor only supports Sparc Solaris (not $arch)])
  fi
  if test "$osrelease" = "5.6" ; then
    _cv_opsys_vers="SOLARIS26"
    _cv_sysname="sun4x_56"
  elif test "$osrelease" = "5.7" ; then
    _cv_opsys_vers="SOLARIS27"
    _cv_sysname="sun4x_57"
  elif test "$osrelease" = "5.8" ; then
    _cv_opsys_vers="SOLARIS28"
    _cv_sysname="sun4x_58"
  elif test "$osrelease" = "5.9" ; then
    _cv_opsys_vers="SOLARIS29"
    _cv_sysname="sun4x_59"
  fi

elif test $os = "IRIX" ; then
  _cv_arch="SGI"
  _cv_opsys="IRIX"
  if test "$osrelease" = "6.5" ; then
    _cv_opsys_vers="IRIX65"
    _cv_sysname="sgi_65"
    IsMipsVersion=3
    IsMipsABI=N32
  else
    _cv_opsys_vers="IRIX62"
    _cv_sysname="sgi_62"
    IsMipsVersion=2
    IsMipsABI=O32
  fi

elif test $os = "AIX" ; then
  # add detection to this when ia64 is needed
  _cv_arch="PPC"
  _cv_opsys="AIX"
  if test "$osversion" = "5" ; then
    _cv_opsys_vers="AIX5"
    _cv_sysname="ppc_aix5"
  fi

elif test $os = "Darwin" ; then
  _cv_arch="PPC"
  _cv_opsys="BSD"
  _cv_opsys_vers="DARWIN"
  _cv_sysname="ppc_macosx"

else
  AC_MSG_ERROR([Condor has not been ported to this platform])
fi

AC_MSG_CHECKING([architecture])
AC_MSG_RESULT([$_cv_arch])
AC_SUBST(_cv_arch,$_cv_arch)

AC_MSG_CHECKING([generic operating system])
AC_MSG_RESULT([$_cv_opsys])
AC_SUBST(_cv_opsys,$_cv_opsys)

AC_MSG_CHECKING([operating system version])
AC_MSG_RESULT([$_cv_opsys_vers])
AC_SUBST(_cv_opsys_vers,$_cv_opsys_vers)

AC_MSG_CHECKING([system identifying string])
AC_MSG_RESULT([$_cv_sysname])
AC_SUBST(_cv_sysname,$_cv_sysname)

if test "$_cv_opsys" = "LINUX" ; then 
  AC_MSG_CHECKING([exact libc version])
  for _libc in "/lib/libc.so.6" "/lib/libc.so.6.1" ; do
    if test -f "$_libc" && test -L "$_libc" ; then 
      _libc_ver=`echo $_libc|perl -nle '$f=readlink;$f=~/libc-(.*)\.so/;print $1;'`
      break;
    elif test -f "$_libc" ; then
      # ugh!  it's not a symlink to a file named with the version, so
      # we try an alternative method.  this is mostly a hack based on
      # crude experimentation.  we run nm on the libc.so, search for
      # "A" symbols with "GLIBC" in them, and grab the *second to*
      # last one we find.  based on testing this method on some
      # versions of glibc where the version was in the filename, this
      # seems to give us the closest match to reality.  it'll
      # certainly work to give us the major + minor number, which is
      # mostly all we care about at this point...
      _libc_ver=`nm $_libc|grep GLIBC|grep A|tail -2|head -1|awk '{print $3}'|cut -f2 -d_`
      break;
    fi
  done
  if test "x$_libc_ver" = "x" ; then
    AC_MSG_RESULT([ERROR])
    AC_MSG_ERROR([Can not determine libc version on this machine])
  fi
  AC_MSG_RESULT([$_libc_ver])
  _cv_libc_kind="GLIBC"
  _cv_libc_major=`echo $_libc_ver | cut -f1 -d.`
  _cv_libc_minor=`echo $_libc_ver | cut -f2 -d.`
  _cv_libc_patch=`echo $_libc_ver | cut -f3 -d.`

  _cv_libc_full=`echo $_cv_libc_kind $_cv_libc_major $_cv_libc_minor $_cv_libc_patch|perl -ane 'print "#define IS_".join('_',@F)." YES";'`
  _cv_libc_majmin="#define IS_$_cv_libc_kind$_cv_libc_major$_cv_libc_minor YES"
  _cv_libc_is_kind="#define IS_$_cv_libc_kind YES"
else
  _cv_libc_full=""
  _cv_libc_majmin=""
  _cv_libc_is_kind=""
fi
AC_SUBST(_cv_libc_full,$_cv_libc_full)
AC_SUBST(_cv_libc_majmin,$_cv_libc_majmin)
AC_SUBST(_cv_libc_is_kind,$_cv_libc_is_kind)


############################################################
# Check for command-line arguments and environment variables
############################################################

AC_ARG_WITH(externals,
 AC_HELP_STRING([--with-externals=DIR],
   [Root of directory tree for external programs needed for building Condor (default is to search in ../externals)]),
 [if test $withval = no; then
    AC_MSG_WARN([You can not use --without-externals or use "no"])
    AC_MSG_ERROR([--with-externals must define a directory (see --help)])
  fi
  CONDOR_VERIFY_EXTERNALS_DIR([$withval],
    [directory specified with --with-externals ($withval) is not valid])
 ],
 [
  # nothing specified, try ../externals.  however, instead of using
  # "`pwd`/../externals", we use AS_DIRNAME to strip off the "src"
  # part of `pwd` and then tack on "/externals".  This doesn't get
  # things right if there are weird symlinks going on, but it wouldn't
  # work if we were using "../externals", either.  In the general case
  # where there's a real parent directory holding src, config and
  # externals, everything works great, and using this as a default
  # makes it possible to just run "./configure" most of the time.
  _cur_pwd=`pwd`
  _root_dir=`AS_DIRNAME(["$_cur_pwd"])`
  _local_ext_dir="$_root_dir/externals"
  if test -d $_local_ext_dir ; then 
    CONDOR_VERIFY_EXTERNALS_DIR([$_local_ext_dir],
    [$_local_ext_dir exists but is not a valid directory tree for external programs needed to build Condor, you must use --with-externals=DIR to specify the path])
  else
    # try /p/condor/workspaces/externals for builds at UW-Madison CS
    _uw_cs_externals="/p/condor/workspaces/externals"
    CONDOR_VERIFY_EXTERNALS_DIR([$_uw_cs_externals],
    [neither $_local_ext_dir (../externals) nor $_uw_cs_externals is a valid directory tree for external programs needed to build Condor, you must use --with-externals=DIR to specify the path])
  fi
 ]
)
AC_SUBST(ext_root,$ac_cv_externals)

# Now, try to find condor_analyze binaries.  In any directory we're
# searching, we look for condor_analyze.(strip|static|release).  If
# any type-specific version doesn't exist, we look for
# "condor_analyze" and try to use that.  We first see if the
# --with-condor_analyze command-line option is set, and if so, look in
# there.  If that fails for any reason, we treat it as a fatal error.
# If that's not set, we look in <externals>/install/condor_analyze.
# If we find anything, we use it, otherwise, we look in
# /usr/local/condor/analyze.  If we still didn't find anything, we
# just print a message, assume the user doesn't want to worry about
# condor_analyze, and set HAS_CONDOR_ANALYZE to "NO" in externals.cf.
AC_MSG_CHECKING([for condor_analyze])
ac_condor_analyze_dir=""
ac_has_condor_analyze=NO
AC_ARG_WITH(condor_analyze,
 AC_HELP_STRING([--with-condor_analyze=DIR],
 [full path to the directory containing pre-compiled versions of condor_analyze to include in the Condor release tarballs (default is to search <externals>/install/condor_analyze, then if nothing was found, /usr/local/condor/analyze)]),
 [if test $withval = no ; then
    ac_has_condor_analyze=NO
  else
    if test -d $withval ; then
      CONDOR_VERIFY_ANALYZE_DIR([$withval])
      if test $ac_has_condor_analyze = "NO" ; then
        AC_MSG_RESULT([ERROR])
        AC_MSG_ERROR([directory specified with --with-condor_analyze ($withval) does not contain any condor_analyze binaries])
      fi 
    else
      AC_MSG_RESULT([ERROR])
      AC_MSG_ERROR([directory specified with --with-condor_analyze ($withval) does not exist])
    fi
  fi
 ],
 [
  CONDOR_VERIFY_ANALYZE_DIR([$ac_cv_externals/install/condor_analyze])
  if test $ac_has_condor_analyze = "NO" ; then
    CONDOR_VERIFY_ANALYZE_DIR([/usr/local/condor/analyze])
  fi
 ]
)
AC_SUBST(has_condor_analyze,$ac_has_condor_analyze)
if test $ac_has_condor_analyze = "YES" ; then
  AC_MSG_RESULT([$ac_condor_analyze_dir])
  CONDOR_SET_ANALYZE([release],fatal)
  CONDOR_SET_ANALYZE([strip],fatal)
  CONDOR_SET_ANALYZE([static],no)
else
  AC_MSG_RESULT([no])
fi


AC_ARG_VAR(TMPDIR, [Directory for temp files during Condor build [/tmp]])
if test "$ac_cv_env_TMPDIR_set" != set; then
    TMPDIR=/tmp
fi
if test ! -d $TMPDIR || test ! -w $TMPDIR; then
    AC_MSG_ERROR([TMPDIR ($TMPDIR) is not a writable directory])
fi

AC_ARG_WITH(purecachedir,
 AC_HELP_STRING([--with-purecachedir=DIR],
  [cache directory for objects instrumented with Purify (default is $TMPDIR)]),
 [ac_cv_purecachedir=$withval],
 [ac_cv_purecachedir="$TMPDIR/$USER/.pcache"]
)
AC_SUBST(pure_cache_dir,$ac_cv_purecachedir)


############################################################
# checks for programs
############################################################

#####################
# compiler detection
#####################

AC_LANG(C)
AC_PROG_CC
if test $ac_cv_c_compiler_gnu != yes; then
   AC_MSG_ERROR( [GNU CC (gcc) is required] )
fi   

# Figure out the version of gcc. 
# This probably seems like a job for a regular expression, not a big
# case statement.  however, in our experience, there's enough variation
# in how these version strings look that explicitly listing the ones
# we recognize seems to work better than trying to figure it all out
# automatically.  moreover, porting Condor to a new version of gcc is
# always a lot of work, so if someone is trying to build on an
# supported gcc version, we might as well catch them here, before they
# get very far.
AC_MSG_CHECKING([for exact version of gcc])
gcc_vers=`gcc -v 2>&1 | tail -1 | perl  -ne '{/.*gcc version\s*(\S*).*/; print "$1\n"}'`

case "$gcc_vers" in
 "egcs-2.91.66" )
    CompilerKind="EGCS"
    CompilerMajor="2"
    CompilerMinor="91"
    CompilerPatch="66"
    ;;
 "2.95.3" )
    CompilerKind="GCC"
    CompilerMajor="2"
    CompilerMinor="95"
    CompilerPatch="3"
    ;;
 "2.95.4" )
    CompilerKind="GCC"
    CompilerMajor="2"
    CompilerMinor="95"
    CompilerPatch="4"
    ;;
 "2.96" )
    CompilerKind="GCC"
    CompilerMajor="2"
    CompilerMinor="96"
    CompilerPatch="0"
    ;;
 "3.0" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="0"
    CompilerPatch="0"
    ;;
 "3.1" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="1"
    CompilerPatch="0"
    ;;
 "3.2" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="2"
    CompilerPatch="0"
    ;;
 "3.2.1" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="2"
    CompilerPatch="1"
    ;;
 "3.2.2" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="2"
    CompilerPatch="2"
    ;;
 "3.3" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="0"
    ;;
 * )
   AC_MSG_RESULT([ERROR])
   AC_MSG_ERROR([Condor will not compile with gcc version $gcc_vers])
   ;;
esac
AC_MSG_RESULT([$gcc_vers])

AC_SUBST(CompilerKind,$CompilerKind)
AC_SUBST(CompilerMajor,$CompilerMajor)
AC_SUBST(CompilerMinor,$CompilerMinor)
AC_SUBST(CompilerPatch,$CompilerPatch)

# Find libgcc.a (needed for all platforms)
GET_GCC_VALUE([libgcc.a],[-print-libgcc-file-name],[gcc_libc])
GET_GCC_VALUE([libstdc++.a],[--print-file-name=libstdc++.a],[lib_std_cpp])

# Find/define crt[01].o for each architecture we support
AC_MSG_CHECKING([for crt0.o])
case "$os" in 
  "SunOS" | "Linux" )
    CRT_BEGIN=`gcc -print-file-name=crt1.o`
    ;;
  "HP-UX" )
    if test $os = "HP-UX" && test "$osrelease" = "B.10.20" ; then
      CRT_BEGIN=/lib/crt0.o
    elif test $os = "HP-UX" && test "$osversion" = "B.11.00" ; then
      # we support only 64-bit compilations of condor on hpux 11.00
      CRT_BEGIN=/usr/ccs/lib/pa20_64/crt0.o
    fi
    ;;
  "IRIX" )
    case "$IsMipsABI" in
      "N64" )
        IrixBaseLibPath=/usr/lib64
        ;;
      "N32" )
        IrixBaseLibPath=/usr/lib32
        ;;
      "O32" )
        IrixBaseLibPath=/usr/lib32
        ;;
    esac
    case "$IsMipsVersion" in
      "2" )
        IrixLibPath=$IrixBaseLibPath
        ;;
      "3" )
        IrixLibPath="$IrixBaseLibPath/mips3"
        ;;
      "4" )
        IrixLibPath="$IrixBaseLibPath/mips4"
        ;;
    esac
    CRT_BEGIN="$IrixLibPath/crt1.o"
    ;;

  "OSF1" )
    CRT_BEGIN=/usr/lib/cmplrs/cc/crt0.o
    ;;

  "AIX" )
    CRT_BEGIN=/lib/crt0.o
    ;;
esac
AC_MSG_RESULT([$CRT_BEGIN])
AC_SUBST([crt_begin],[$CRT_BEGIN])

AC_LANG(C++)
AC_PROG_CXX
if test $ac_cv_cxx_compiler_gnu != yes; then
   AC_MSG_ERROR( [GNU C++ (g++) is required] )
fi   

#########
# figure out what linker flags put us in static-link mode
#########
gcc_ld_prog=`gcc -print-prog-name=ld`
AC_MSG_NOTICE([checking what gcc flag forces $gcc_ld_prog into static mode])
echo "main() { return 0; }" > static_test.c
gcc -c static_test.c > /dev/null 2>&1
gcc_static_ld_flag=""
#####
# -Bstatic is GNU ld
# -B,static is Solaris ld
# -a,archive is HPUX ld
# -no_so is DUX ld
#####
for arg in "-Bstatic" "-B,static" "-a,archive" "-no_so" ; do
  # we always want to prepend "-Wl,"
  arg="-Wl,$arg"
  AC_MSG_CHECKING([$arg])    
  _test_output=`gcc -o static_test $arg static_test.o 2>&1`
  _test_status=$?
  if test $_test_status -eq 0 && test "x$_test_output" = "x"; then 
    gcc_static_ld_flag=$arg
    AC_MSG_RESULT([yes])
    break
  else
    AC_MSG_RESULT([no])
  fi
done
rm -f static_test.c static_test.o static_test > /dev/null 2>&1
if test "x$gcc_static_ld_flag" = "x" ; then
  msg="can not find appropriate flags for $gcc_ld_prog!"
  if test $os = "SunOS" ; then
    # on Solaris, this is fatal, since we depend on this info
    AC_MSG_ERROR([$msg])
  else
    AC_MSG_WARN([$msg])
  fi
else 
  AC_SUBST([gcc_static_ld_flag],[$gcc_static_ld_flag])
fi
  

#########################################
# vendor compilers (for the test suites)
#########################################
AC_MSG_NOTICE([checking for vendor compilers (used by Condor test suites)])

# we always look for g77, since we should have at least this,
# regardless of what platform we are...
AC_PATH_PROG([_cv_gnu_g77],[g77],[none],[$PATH])
if test "$_cv_gnu_g77" = "none" ; then
  _cv_has_gnu_g77="NO"
else
  _cv_has_gnu_g77="YES"
fi

# now, look for everything else, depending on our platform.  if it's
# Linux, we know there are no vendor compilers to worry about, so we
# can bail without doing any real work.  otherwise, we have to search
# for what we might be able to use...
if test "$os" = "Linux" ; then
  AC_SUBST([_cv_vendor_cc],[""])
  AC_SUBST([_cv_vendor_cplus],[""])
  AC_SUBST([_cv_vendor_cplus_name],[""])
  AC_SUBST([_cv_vendor_f77],[""])
  AC_SUBST([_cv_vendor_f90],[""])
  _cv_has_vendor_cc="NO"
  _cv_has_vendor_cplus="NO"
  _cv_has_vendor_f77="NO"
  _cv_has_vendor_f90="NO"
else
  if test "$os" = "HP-UX" ; then
    _vendor_cc_PATH="/usr/bin:$PATH"
  else
    _vendor_cc_PATH="$PATH:/opt/SUNWspro/bin:/usr/ucb"
  fi
  AC_PATH_PROG([_cv_vendor_cc],[cc],[none],[$_vendor_cc_PATH])
  if test "$_cv_vendor_cc" = "none" ; then
    _cv_has_vendor_cc="NO"
  else
    _cv_has_vendor_cc="YES"
  fi

  AC_PATH_PROGS([_cv_vendor_cplus],[CC cxx],[none],
                [$PATH:/opt/SUNWspro/bin:/usr/ucb])
  if test "$_cv_vendor_cplus" = "none" ; then
    _cv_has_vendor_cplus="NO"
  else
    _cv_has_vendor_cplus="YES"
    _cv_vendor_cplus_name=`basename $_cv_vendor_cplus`
      AC_SUBST([_cv_vendor_cplus_name],[$_cv_vendor_cplus_name])
  fi

  AC_PATH_PROG([_cv_vendor_f77],[f77],[none],[$PATH])
  if test "$_cv_vendor_f77" = "none" ; then
    _cv_has_vendor_f77="NO"
  else
    _cv_has_vendor_f77="YES"
  fi

  AC_PATH_PROG([_cv_vendor_f90],[f90],[none],
               [$PATH:/opt/SUNWspro/bin:/usr/ucb])
  if test "$_cv_vendor_f90" = "none" ; then
    _cv_has_vendor_f90="NO"
  else
    _cv_has_vendor_f90="YES"
  fi
fi
AC_SUBST([has_gnu_g77],[$_cv_has_gnu_g77])
AC_SUBST([has_vendor_cc],[$_cv_has_vendor_cc])
AC_SUBST([has_vendor_cplus],[$_cv_has_vendor_cplus])
AC_SUBST([has_vendor_f77],[$_cv_has_vendor_f77])
AC_SUBST([has_vendor_f90],[$_cv_has_vendor_f90])


#####################
# everything else
#####################

CHECK_PROG_IS_GNU(MAKE, make)
if test $_cv_make_is_gnu = no; then
   AC_MSG_ERROR( [GNU make is required] )
fi

AC_CHECK_PROG(ac_patch,patch,yes,no)
if test $ac_patch = no; then
    AC_MSG_ERROR( [patch is required] )
fi
# now, make sure patch supports unified diffs
AC_CACHE_CHECK([if patch supports unified diffs],_cv_unified_patch,
 [_cv_unified_patch="no" ;
  ( patch --unified < /dev/null > /dev/null 2>&1 )
  _patch_status=$?
  if test $_patch_status -ne 0 ; then
    _cv_unified_patch="no";
  else 
    _cv_unified_patch="yes";
  fi
 ]
)
if test $_cv_unified_patch != "yes"; then
   AC_MSG_ERROR( [patch must support unified diffs] )
fi

AC_PROG_LEX
AC_CHECK_PROG(ac_lex,lex,yes,no)
if test $ac_lex = no; then
    AC_MSG_ERROR( [lex is required] )
fi

AC_CHECK_PROG(ac_yacc,yacc,yes,no)
if test $ac_yacc = no; then
    AC_MSG_ERROR( [yacc is required] )
fi

# ar is used for numerous libraries in Condor, and for extracting
# objects from the system libc.a for the Condor syscall library.
AC_CHECK_PROG(ac_ar,ar,yes,no)
if test $ac_ar = no; then
    AC_MSG_ERROR( [ar is required] )
fi

# strip is used in the packaging of Condor for all released binaries
# we require GNU strip since it does a much better job of shrinking
# the size of our binaries, given that they're all built with GNU CC.
# however, on OSX, there's no such thing as GNU strip, so we leave
# that alone for now...
CHECK_PROG_IS_GNU(STRIP, strip)
if test "$_cv_strip_is_gnu" = "no"; then
   case `uname -s` in 
   "Darwin" )
      AC_MSG_WARN( [GNU strip is prefered, but OSX doesn't have it] ) ;;

   "OSF1" ) 
      if test `uname -r` != "V5.1" ; then
         AC_MSG_ERROR( [GNU strip is required] )
      fi ;; 

   * )
      AC_MSG_ERROR( [GNU strip is required] ) ;;
   esac
else
   if test `uname -s` = "OSF1" && test `uname -r` = "V5.1" ; then
      AC_MSG_ERROR( [GNU strip will not work on DUX 5.1] )
   fi
fi

# We depend on some GNU-specific options to tar.  We also depend on
# using --owner and --group, and want those to set the ownerships of
# all files in the tarballs we create to 0.0 (root).  however, not all
# versions of tar support --group=0, and not all machines have group
# root. :( so, we test for what we need, and use whatever works.
# also, some sites have different GNU tar installed under a different
# name, so we've got to do some magic to handle that, too.
AC_CACHE_CHECK([for GNU tar],_cv_gnu_tar,
 [_cv_gnu_tar="not found" ;
  for a in "$TAR" tar gtar gnutar ; do
    if test -z "$a" ; then continue ; fi ;
    if ( sh -c "$a --version" 2> /dev/null | grep GNU  2>&1 > /dev/null );
    then
      _cv_gnu_tar=$a;
      break;
    fi
  done;
 ]
)
if test "$_cv_gnu_tar" = "not found" ; then
   AC_MSG_ERROR([GNU tar is required])
fi
# if we got this far, we found a version of GNU tar.  now, see what
# options work to make tarballs with files owned by 0.0:
TarRootCmd=''
CHECK_TAR_OPTION([--group=0], [_cv_gnu_tar_group_zero])
if test "$_cv_gnu_tar_group_zero" = "yes"; then
  # we can assume if tar supports "--group=0", "--owner=0" will work.
  TarCmd="$_cv_gnu_tar --owner=0 --group=0"
else
  CHECK_TAR_OPTION([--group=root],[_cv_gnu_tar_group_root])
  CHECK_TAR_OPTION([--owner=root],[_cv_gnu_tar_owner_root])
  if test "$_cv_gnu_tar_group_root" = "yes"; then
     if test "$_cv_gnu_tar_owner_root" = "yes"; then
       TarCmd="$_cv_gnu_tar --owner=root --group=root"
     else
       tar_err_msg="user called \"root\" with UID 0"
     fi
  else
    # group root didn't work, see if owner root did so we can
    # construct the appropriate error message
    if test "$_cv_gnu_tar_owner_root" = "yes"; then
      tar_err_msg="group called \"root\" with GID 0"
    else
      tar_err_msg="user and group called \"root\" with UID 0 and GID 0"
    fi
  fi
fi
if test "x$TarCmd" = "x"; then
  AC_MSG_ERROR([you system does not support tar features Condor requires for packaging our releases.  you must install a version of GNU tar which supports numeric arguments to --group and --owner (such as version 1.13 or higher), or add a $tar_err_msg, then re-run configure])
fi
AC_SUBST(TarCmd,$TarCmd)

# gzip is required for packaging up Condor releases. 
AC_CHECK_PROG(ac_gzip,gzip,yes,no)
if test $ac_gzip = no; then
    AC_MSG_ERROR( [gzip is required] )
fi

# m4 is used to build PVM
AC_CHECK_PROG(ac_m4prog,m4,yes,no)
if test $ac_m4prog = no; then
    AC_MSG_ERROR( [m4 is required] )
fi

AC_CHECK_PROG(ac_purify,purify,yes,no)
if test "$ac_purify" = yes; then
   AC_MSG_CHECKING([purify cache directory ($ac_cv_purecachedir)])
   if test -d $ac_cv_purecachedir && test -w $ac_cv_purecachedir ; then
      AC_MSG_RESULT( exists )
   else
      AS_MKDIR_P($ac_cv_purecachedir)
      AC_MSG_RESULT( created )
   fi
fi


# checks for libraries

# checks for header files

# checks for types

# checks for structures

# checks for compiler characteristics

# checks for library functions

# checks for system services


############################################################
# Configure-generated output
############################################################
AC_CONFIG_FILES(../config/configure.cf ../config/externals.cf)
AC_OUTPUT

# Finally, call Condor's imake script to generate Makefile using imake
# (yes, we know it seems crazy to use configure to setup Imake to make
# Makefiles... we're in transition).
./condor_imake
