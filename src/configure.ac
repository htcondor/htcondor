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
_bad_path_dirs="/s/std/bin /usr/local/bin /usr/bin /bin /usr/ccs/bin"
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
# these three are just used by configure to specify the settings we
# actually care about, described below
arch=`uname -m`
osrelease=`uname -r`
osversion=`uname -v`

# this is what ends up as "Architecture" in the Imakefiles, and is
# used in a number of the IS_* #define's too.  luckily, we can use the
# same thing in both places without confusion...
_cv_arch=""

# this is the opsys-specific string in the IS_* #define's
_cv_opsys=""

# this is what ends up as "OperatingSystem" in the Imakefile, which
# unfortnuately is sometimes different than what we use in _cv_opsys
# this is stupid and evil, but since the naming isn't consistent in
# the Condor code and build system, for now, we'll just define both.
# hopefully, someday relatively soon, we'll be able to go through and
# clean this discrepancy up and remove this evil setting.  sorry.
_cv_op_sys=""

# the version-specific opsys string we use in the IS_* #define's
_cv_opsys_vers=""

# what gets set as the "SYSNAME" make variable, for now, it's only
# used as the argument to "make_final_tarballs" to determine what the
# packaged filenames should be when we try to release this platform
_cv_sysname=""

# Make sure if we get back something that begins with "IRIX" that we
# strip off anything after it (e.g. "IRIX64", etc).
os=`uname -s | sed 's/IRIX.*/IRIX/'`

if test "$os" = "HP-UX"; then
  _cv_op_sys="HPUX"
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
  _cv_op_sys="OSF1"
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
  _cv_op_sys="LINUX"
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
       _cv_linux_distro="RH-UNKNOWN"
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
       _cv_linux_distro="SuSE-UNKNOWN"
       ;;
    esac
  elif test -f "/etc/debian_version" ; then
    _deb_vers=`cat /etc/debian_version | sed 's/\./_/g'`;
    _cv_linux_distro="DEBIAN-`echo _deb_vers`"
    AC_MSG_WARN([Condor is not yet ported to Debian Linux distributions])
    AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
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
  _cv_op_sys="Solaris"
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
  _cv_op_sys="IRIX"
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
  _cv_op_sys="AIX"
  _cv_opsys="AIX"
  if test "$osversion" = "5" ; then
    _cv_opsys_vers="AIX5"
	if test "$osrelease" = "1" ; then
      _cv_sysname="ppc_aix51"
	elif test "$osrelease" = "2" ; then
      _cv_sysname="ppc_aix52"
	else
      _cv_sysname="ppc_aix5unknown"
      AC_MSG_WARN([Unrecognised AIX version])
      AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
	fi
  else
    _cv_opsys_vers="AIXUNKNOWN"
    _cv_sysname="ppc_aixunknown"
    AC_MSG_WARN([Unrecognised AIX version])
    AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
  fi

elif test $os = "Darwin" ; then
  _cv_arch="PPC"
  _cv_op_sys="Darwin"
  _cv_opsys="DARWIN"
  if test "$osrelease" = "6.8" ; then
    _cv_opsys_vers="DARWIN-6_8"
    _cv_sysname="ppc_macosx"
  elif test "$osrelease" = "7.0.0" ; then
    _cv_opsys_vers="DARWIN-7_0"
    _cv_sysname="ppc_macosx_7_0"
  else
    _cv_opsys_vers="DARWIN-UNKNOWN"
    AC_MSG_WARN([Unrecognised MacOSX/Darwin version])
    AC_MSG_WARN([You may have trouble building and/or packaging Condor]) 
  fi  


else
  AC_MSG_ERROR([Condor has not been ported to this platform])
fi

AC_MSG_CHECKING([architecture])
AC_MSG_RESULT([$_cv_arch])
AC_SUBST(_cv_arch,$_cv_arch)

AC_MSG_CHECKING([generic operating system])
AC_MSG_RESULT([$_cv_opsys])
AC_SUBST(_cv_opsys,$_cv_opsys)
AC_SUBST(_cv_op_sys,$_cv_op_sys)

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
  AC_MSG_RESULT([GLIBC $_libc_ver])
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


#########
# See what directory we're being run in, since we need this info for
# various other things.  however, instead of using "`pwd`/..", we use
# AS_DIRNAME to strip off the "src" part of `/bin/pwd`.  This should
# get things right, even if there are weird symlinks going on.
#########
_src_dir=`/bin/pwd`
_root_dir=`AS_DIRNAME(["$_src_dir"])`

CONDOR_VERIFY_CONFIG_DIR([$_root_dir/config],
                         [config directory is not valid])
AC_SUBST(_cv_ext_config_sh,"$_root_dir/config/config.sh") 


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
  # nothing specified, try ../externals.  using this as a default
  # makes it possible to just run "./configure" most of the time.
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
 "3.3.1" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="1"
    ;;
 "3.3.2" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="2"
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
# this gcc3+ specific library is used for standard universe linking
if test "X$CompilerKind" = "XGCC" && test "X$CompilerMajor" = "X3" ; then
  GET_GCC_VALUE([libgcc_eh.a],[--print-file-name=libgcc_eh.a],[lib_gcc_eh])
fi

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
gcc_static_ld_flag=""
# determine the flags...
if test $os = "AIX" ; then
  # AIX static linking with respect to Condor is not pretty. In effect,
  # we must perform a dynamic link ALWAYS, except in the case of the 
  # C++ compiler support libraries, which must be linked in statically
  # so the executables can be run on machines that they weren't built on.
  AC_MSG_NOTICE([using a pre-determined AIX static linking algorithm])
else
  # make a trivial test program and compile it to a .o...
  echo "main() { return 0; }" > static_test.c
  gcc -c static_test.c > /dev/null 2>&1
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
fi
  

#########################################
# vendor compilers (for the test suites)
#########################################
AC_MSG_NOTICE([checking for vendor compilers (used by Condor test suites)])

# we always look for g77, since we should have at least this,
# regardless of what platform we are...
AC_PATH_PROG([_cv_gnu_g77],[g77],[no],[$PATH])
if test "$_cv_gnu_g77" = "no" ; then
  _cv_has_gnu_g77="NO"
else
  _cv_has_gnu_g77="YES"
fi

# now, look for everything else, depending on our platform.  if it's
# Linux or Darwin, we know the "vendor" compilers are really gcc, so
# we can bail without doing any real work.  otherwise, we have to
# search for what we might be able to use...
if test "$os" = "Linux" || test "$os" = "Darwin" ; then
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
  AC_PATH_PROG([_cv_vendor_cc],[cc],[no],[$_vendor_cc_PATH])
  if test "$_cv_vendor_cc" = "no" ; then
    _cv_has_vendor_cc="NO"
  else
    _cv_has_vendor_cc="YES"
  fi

  AC_PATH_PROGS([_cv_vendor_cplus],[CC cxx],[no],
                [$PATH:/opt/SUNWspro/bin:/usr/ucb])
  if test "$_cv_vendor_cplus" = "no" ; then
    _cv_has_vendor_cplus="NO"
  else
    _cv_has_vendor_cplus="YES"
    _cv_vendor_cplus_name=`basename $_cv_vendor_cplus`
      AC_SUBST([_cv_vendor_cplus_name],[$_cv_vendor_cplus_name])
  fi

  AC_PATH_PROG([_cv_vendor_f77],[f77],[no],[$PATH])
  if test "$_cv_vendor_f77" = "no" ; then
    _cv_has_vendor_f77="NO"
  else
    _cv_has_vendor_f77="YES"
  fi

  AC_PATH_PROG([_cv_vendor_f90],[f90],[no],
               [$PATH:/opt/SUNWspro/bin:/usr/ucb])
  if test "$_cv_vendor_f90" = "no" ; then
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

AC_CHECK_PROG(ac_patch,patch,patch,no)
if test $ac_patch = no; then
    AC_MSG_ERROR( [patch is required] )
fi
# now, make sure patch supports unified diffs
AC_CACHE_CHECK([if patch supports unified diffs],_cv_unified_patch,
 [_cv_unified_patch="no" ;
  _cv_unified_patch="no"
 cat > conftest_file1 << _EOF
this
is
a test
hello world
_EOF
 cat > conftest_patch << _EOF
--- conftest_file1 Wed Nov 26 20:04:43 2003
+++ conftest_file2 Wed Nov 26 20:01:25 2003
@@ -1,4 +1,4 @@
 this
 is
-a test
+A TEST
 hello world
_EOF
  patch < conftest_patch > /dev/null 2>&1
  _patch_status=$?
  if test $_patch_status -eq 0 ; then
    # patch didn't return failure, but it still might not have
    # worked.  so, see if the change actually happened or not.
    grep "TEST" conftest_file1 > /dev/null 2>&1
    _grep_status=$?
    if test $_grep_status -eq 0 ; then
      _cv_unified_patch="yes";
    fi
  fi
  rm -f conftest_file1 conftest_patch
 ]
)
if test $_cv_unified_patch != "yes"; then
   AC_MSG_NOTICE( [try installing GNU patch] )
   AC_MSG_ERROR( [patch must support unified diffs] )
fi

AC_PROG_LEX
if test "$LEX" = "no" ; then
    AC_MSG_ERROR( [lex is required] )
fi


# We don't want to do the default AC_PROG_YACC, since it selects
# "bison -y" if it can find it, and that doesn't work for the yacc
# input in the Condor build.  So, we just manually check for byacc as
# our first choice, then regular old yacc...
AC_CHECK_PROG(ac_byacc,byacc,byacc,no)
if test $ac_byacc = "byacc"; then
  YACC=$ac_byacc
else
  AC_CHECK_PROG(ac_yacc,yacc,yacc,no)
  if test $ac_yacc = no; then
    AC_MSG_ERROR( [byacc or yacc is required] )
  else 
    YACC=$ac_yacc
  fi
fi
AC_SUBST([YACC],[$YACC])

# ar is used for numerous libraries in Condor, and for extracting
# objects from the system libc.a for the Condor syscall library.
AC_CHECK_PROG(ac_ar,ar,ar,no)
if test $ac_ar = no; then
    AC_MSG_ERROR( [ar is required] )
fi

# strip is used in the packaging of Condor for all released binaries.
# We want to see if we can find GNU strip since it usually does a much
# better job of shrinking the size of our binaries, given that they're
# all built with GNU CC/C++.  however, on OSX, there's no such thing
# as GNU strip.  Also, on other platforms, even if we find GNU strip,
# it might not work properly.  We search for GNU strip, and if we find
# it, we see if it works.  Then we search for a non-GNU vendor strip,
# and see if it works.  If we only found 1 working version, we use it.
# If both versions exist and both work, we see which one produced the
# smallest output program and use that.
AC_MSG_CHECKING([for GNU strip]) 
_cv_gnu_strip="no"
_cv_gnu_strip_works="no"
_try_gnu_strip="$STRIP strip gstrip gnustrip"
as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
for as_dir in $PATH ; do
  IFS=$as_save_IFS
  for a in $_try_gnu_strip ; do
    _try="$as_dir/$a"
    if ( sh -c "$_try --version" 2> /dev/null | grep GNU 2>&1 > /dev/null );
    then
      AC_MSG_RESULT([$_try])
      _cv_gnu_strip="$_try";
      break;
    fi
  done;
  if ! test "$_cv_gnu_strip" = "no" ; then
    break;
  fi
done;
if test "$_cv_gnu_strip" = "no" ; then
  AC_MSG_RESULT([no]) 
  if ! test "$os" = "Darwin" && ! test "$os" = "OSF1" ; then 
    AC_MSG_WARN([GNU strip is recommended]) 
  fi
else
  CONDOR_TEST_STRIP([GNU strip], [_cv_gnu_strip]) 
  if test "$_cv_gnu_strip_works" = "no" ; then
    AC_MSG_WARN([GNU strip is installed but doesn't work]) 
  fi
fi

AC_MSG_CHECKING([for vendor strip]) 
_cv_vendor_strip="no"
_try_vendor_strip="$STRIP strip"
as_save_IFS=$IFS; IFS=$PATH_SEPARATOR
for as_dir in $PATH ; do
  IFS=$as_save_IFS
  for a in $_try_vendor_strip ; do
    _try="$as_dir/$a"
    if ( sh -c "$_try --version" 2>/dev/null | grep GNU 2>&1 >/dev/null );
    then
      continue;
    fi
    if test -x "$_try" ; then 
      AC_MSG_RESULT([$_try])
      _cv_vendor_strip="$_try";
      break;
    fi
  done;
  if ! test "$_cv_vendor_strip" = "no" ; then
    break;
  fi
done;
if test "$_cv_vendor_strip" = "no" ; then
  AC_MSG_RESULT([no]) 
else
  CONDOR_TEST_STRIP([vendor strip], [_cv_vendor_strip]) 
fi

AC_MSG_CHECKING([which strip we should use])
# now, based on all the tests, figure out the best one to use:
if test "$_cv_gnu_strip_works" = "no" ; then
  if test "$_cv_vendor_strip_works" = "no" ; then
    AC_MSG_RESULT( [none available] )
    AC_MSG_ERROR( [strip is required] )
  else
    # no GNU, but there is a vendor, we'll just use it.
    STRIP=$_cv_vendor_strip
  fi
else
  if test "$_cv_vendor_strip" = "no" ; then
    # no vendor, but we have a working GNU, just use it
    STRIP=$_cv_gnu_strip
  else
    # have both versions and both work, see which one does a better
    # job of shriking the output.  in case of a tie, use vendor strip
    if test "$_cv_gnu_strip_outsize" -lt "$_cv_vendor_strip_outsize" ; 
    then
      STRIP=$_cv_gnu_strip
    else
      STRIP=$_cv_vendor_strip
    fi
  fi
fi
AC_MSG_RESULT([$STRIP])
AC_SUBST([STRIP],[$STRIP])

# We depend on some GNU-specific options to tar.  We also depend on
# using --owner and --group, and want those to set the ownerships of
# all files in the tarballs we create to 0.0 (root).  however, not all
# versions of tar support --group=0, and not all machines have group
# root. :( so, we test for what we need, and use whatever works.
# also, some sites have different GNU tar installed under a different
# name, so we've got to do some magic to handle that, too.
AC_CACHE_CHECK([for GNU tar],_cv_gnu_tar,
 [_cv_gnu_tar="no" ;
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
if test "$_cv_gnu_tar" = "no" ; then
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

# We use "cp -r" to package parts of Condor.  However, we need to make
# sure that it dereferences symlinks, instead of copying the links.
# GNU cp copies symlinks by default with -r, so we need to add "-Lr"
# to get the behavior we want.  Unfortunately, the vendor cp on many
# platforms doesn't recognize "-L".  So, we just do some tests here to
# figure out what flags get the behavior we want, and pass that on to
# the places that need it via the Makefiles.
AC_PATH_PROG([_cv_cp],[cp],[no],[$PATH])
if test "$_cv_cp" = "no" ; then
  AC_MSG_ERROR([no cp in your PATH! Either fix your PATH or your machine])
fi
AC_CACHE_CHECK([what options to cp recursively copy and follow symlinks],
               _cv_cp_flags,
 [_cv_cp_flags="none" ;
  for a in "-rL" "-r" ; do
    CONDOR_TRY_CP_RECURSIVE_SYMLINK_FLAG([$_cv_cp],[$a],[_cv_cp_flags])
    if ! test "$_cv_cp_flags" = "none" ; then
      break;
    fi  
  done;
 ]
)
if test "$_cv_cp_flags" = "none" ; then
  AC_MSG_ERROR([The cp in your PATH does not work, try installing GNU cp])
else
  AC_SUBST(CpCmd,"$_cv_cp $_cv_cp_flags")
fi  

# gzip is required for packaging up Condor releases. 
AC_CHECK_PROG(ac_gzip,gzip,gzip,no)
if test $ac_gzip = no; then
    AC_MSG_ERROR( [gzip is required] )
fi

# m4 is used to build PVM
AC_CHECK_PROG(ac_m4prog,m4,m4,no)
if test $ac_m4prog = no; then
    AC_MSG_ERROR( [m4 is required] )
fi

AC_CHECK_PROG(ac_purify,purify,purify,no)
if test "$ac_purify" = "purify"; then
   AC_MSG_CHECKING([purify cache directory ($ac_cv_purecachedir)])
   if test -d $ac_cv_purecachedir && test -w $ac_cv_purecachedir ; then
      AC_MSG_RESULT( exists )
   else
      AS_MKDIR_P($ac_cv_purecachedir)
      AC_MSG_RESULT( created )
   fi
fi


############################################################
# checks for libraries
############################################################

# The DRMAA C library needs to be compiled with pthreads in mind if
# pthreads are available. This machinery ends up defining HAS_PTHREADS
# In the build system which is then used to add the correct threading
# flags to the compile line for drmaa code.
_cv_has_pthreads=0
AC_CHECK_LIB(pthread, pthread_create, ac_pthreads="yes", ac_pthreads="no")
if test $ac_pthreads = "yes"; then
  _cv_has_pthreads=1
fi
AC_SUBST([has_pthreads],[$_cv_has_pthreads])


############################################################
# checks for header files
############################################################

# Check for sys_types.h
AC_CHECK_HEADER(sys/types.h, _cv_has_sys_types_h="yes", _cv_has_sys_types_h="no")
if test "$_cv_has_sys_types_h" = "yes" ; then
 AC_SUBST(has_sys_types_h,"YES")
else
 AC_SUBST(has_sys_types_h,"NO")
fi

# Check for stdint.h
AC_CHECK_HEADER(stdint.h, _cv_has_stdint_h="yes", _cv_has_stdint_h="no")
if test "$_cv_has_stdint_h" = "yes" ; then
 AC_SUBST(has_stdint_h,"YES")
else
 AC_SUBST(has_stdint_h,"NO")
fi

# Check for inttypes.h
AC_CHECK_HEADER(inttypes.h, _cv_has_inttypes_h="yes", _cv_has_inttypes_h="no")
if test "$_cv_has_inttypes_h" = "yes" ; then
 AC_SUBST(has_inttypes_h,"YES")
else
 AC_SUBST(has_inttypes_h,"NO")
fi


############################################################
# checks for types
############################################################

# Check for int64_t type family
AC_CHECK_TYPE(int64_t, _cv_has_int64_t="yes", _cv_has_int64_t="no")
if test "$_cv_has_int64_t" = "yes" ; then
 AC_SUBST(has_int64_t,"YES")
else
 AC_SUBST(has_int64_t,"NO")
fi

# Check for long long
AC_CHECK_TYPE(long long, _cv_has_longlong="yes", _cv_has_longlong="no")
if test "$_cv_has_longlong" = "yes" ; then
 AC_SUBST(has_longlong,"YES")
else
 AC_SUBST(has_longlong,"NO")
fi

# Check for M$ __int64 type
AC_CHECK_TYPE(__int64, _cv_has___int64="yes", _cv_has___int64="no")
if test "$_cv_has___int64" = "yes" ; then
 AC_SUBST(has___int64,"YES")
else
 AC_SUBST(has___int64,"NO")
fi

# checks for structures

# checks for compiler characteristics

# checks for library functions

# Check for stat64()
AC_CHECK_FUNC(stat64, _cv_has_stat64="yes", _cv_has_stat64="no")
if test "$_cv_has_stat64" = "yes" ; then
 AC_SUBST(has_stat64,"YES")
else
 AC_SUBST(has_stat64,"NO")
fi

# checks for system services

# checks for platform-specific settings we don't yet auto-detect

AC_MSG_CHECKING([if Condor can also build a statically linked release])
case "$os" in
  "HP-UX" )
    if test "x$_cv_opsys_vers" = "xHPUX11" ; then
    # Grrr.... libxnet.so (the XOpen interface) does NOT have a .a version
      _cv_has_static=no
    else
      _cv_has_static=yes
    fi
    ;;
  "OSF1" )
    _cv_has_static=yes
    ;;
  "Linux" )
    _cv_has_static=yes
    ;;
  "SunOS" )
    _cv_has_static=no
    ;;
  "IRIX" )
    _cv_has_static=no
    ;;
  "Darwin" )
    _cv_has_static=no
    ;;
  "AIX" )
    _cv_has_static=no
    ;;
  * )
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([Unrecognized platform: $os])
esac
AC_MSG_RESULT([$_cv_has_static])
if test "$_cv_has_static" = "yes" ; then
  AC_SUBST(_cv_has_static,YES)
else
  AC_SUBST(_cv_has_static,NO)
fi


############################################################
# What versions of what externals do we need
############################################################
AC_MSG_NOTICE([determining versions of external packages])


# For now, we hardcode that non-i386 platforms are clipped and don't
# need an external glibc for anything...
if test "x$_cv_libc_kind" = "xGLIBC" -a "x$_cv_arch" = "xI386" ; then
  AC_MSG_CHECKING([glibc])
  _cv_libc_full_vers="$_cv_libc_major.$_cv_libc_minor.$_cv_libc_patch"
  case "$_cv_libc_full_vers" in
  "2.2.4" )
    _cv_ext_glibc_version="glibc-2.2.2"
    ;;
  "2.2.5" )
    _cv_ext_glibc_version="glibc-2.2.2"
    ;;
  "2.2.93" )
    _cv_ext_glibc_version="glibc-2.2.93"
    ;;
  "2.3.2" )
    # the glibc 2.3.2 from redhat's source rpms are very different than the
	# stock 2.3.2 you can download off of the web.
    _cv_ext_glibc_version="glibc-2.3.2.27.9.7"
    ;;
  * )  
    AC_MSG_RESULT([ERROR])
    AC_MSG_ERROR([Condor does NOT know what glibc external to use with glibc-$_cv_libc_full_vers])
  esac
  AC_MSG_RESULT([$_cv_ext_glibc_version])
  AC_SUBST(ext_glibc_version,$_cv_ext_glibc_version)
  AC_SUBST(want_ext_glibc,YES)
else
  AC_SUBST(want_ext_glibc,NO)
  AC_SUBST(ext_glibc_version,UNUSED)
fi


AC_MSG_CHECKING([kerberos])
if test "$os" = "AIX" ; then
_cv_ext_kerberos_version="krb5-1.3.1"
else
_cv_ext_kerberos_version="krb5-1.2.5"
fi
AC_MSG_RESULT([$_cv_ext_kerberos_version])
AC_SUBST(ext_kerberos_version,$_cv_ext_kerberos_version)


AC_MSG_CHECKING([globus])
_cv_ext_globus_version="globus-2.4.3.vdt.1.1.14"
_cv_ext_globus_data_version="globus-2.4.3.data"
AC_MSG_RESULT([$_cv_ext_globus_version])
AC_SUBST(ext_globus_version,$_cv_ext_globus_version)
AC_SUBST(ext_globus_data_version,$_cv_ext_globus_data_version)

AC_MSG_CHECKING([globus flavor])
_cv_globus_flavor=gcc32dbg
if test "$_cv_arch" = "ALPHA" -o "$_cv_arch" = "IA64" ; then
  _cv_globus_flavor=gcc64dbg
elif test "$_cv_arch" = "SGI" -o "$_cv_opsys" = "AIX" ; then
  _cv_globus_flavor=vendorcc32dbg
fi
AC_MSG_RESULT([$_cv_globus_flavor])
AC_SUBST(globus_flavor,$_cv_globus_flavor)


AC_MSG_CHECKING([gpt])
_cv_ext_gpt_version="gpt-3.0"
AC_MSG_RESULT([$_cv_ext_gpt_version])
AC_SUBST(ext_gpt_version,$_cv_ext_gpt_version)


AC_MSG_CHECKING([gahp])
_cv_ext_gahp_version="gahp-1.0.10"
AC_MSG_RESULT([$_cv_ext_gahp_version])
AC_SUBST(ext_gahp_version,$_cv_ext_gahp_version)


AC_MSG_CHECKING([gt3gahp])
_cv_ext_gt3gahp_version="gt3gahp-1.0"
AC_MSG_RESULT([$_cv_ext_gt3gahp_version])
AC_SUBST(ext_gt3gahp_version,$_cv_ext_gt3gahp_version)


AC_MSG_CHECKING([zlib])
_cv_ext_zlib_version="zlib-1.1.3"
AC_MSG_RESULT([$_cv_ext_zlib_version])
AC_SUBST(ext_zlib_version,$_cv_ext_zlib_version)


AC_MSG_CHECKING([pvm])
_cv_ext_pvm_version="pvm-3.4.2"
AC_MSG_RESULT([$_cv_ext_pvm_version])
AC_SUBST(ext_pvm_version,$_cv_ext_pvm_version)


############################################################
# Configure-generated output
############################################################
AC_CONFIG_FILES( ../config/configure.cf ../config/externals.cf
                 ../config/config.sh )
AC_OUTPUT

# Finally, call Condor's imake script to generate Makefile using imake
# (yes, we know it seems crazy to use configure to setup Imake to make
# Makefiles... we're in transition).
./condor_imake
