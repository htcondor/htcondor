# Configure template for Condor.                             -*-Autoconf-*-
# Process this file with autoconf to produce a configure script.
##**************************************************************
##
## Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
## University of Wisconsin-Madison, WI.
## 
## Licensed under the Apache License, Version 2.0 (the "License"); you
## may not use this file except in compliance with the License.  You may
## obtain a copy of the License at
## 
##    http://www.apache.org/licenses/LICENSE-2.0
## 
## Unless required by applicable law or agreed to in writing, software
## distributed under the License is distributed on an "AS IS" BASIS,
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
## See the License for the specific language governing permissions and
## limitations under the License.
##
##**************************************************************

# We need a recent autoconf for many of the macros and tests we're using
# NOTE: 2.57 has a bug when testing for functions, which we now hit by
# testing for mkstemp(), so we need at least 2.59...
AC_PREREQ(2.59)

# Initialize autoconf
AC_INIT
AC_CONFIG_HEADERS([config.h])

AC_MSG_NOTICE([checking for critical tools used by configure])
AC_PROG_AWK
REQUIRE_PATH_PROG([GREP],[grep])
REQUIRE_PATH_PROG([CUT],[cut])
AC_PATH_PROG([WHICH],[which],[no],[$PATH])

# Condor only requires 5.002, but GPT apparently relies on 5.005_03 or
# greater.  Unfortunately, GPT doesn't have a configure script or
# anything, it just lets perl die with a require statement.  So, we
# test for the right version here and fail in an early and obvious way
# if there's a problem.  Also, we check for this early, so we can use
# perl in the rest of the tests in here, if we want it.
AC_PROG_PERL_VERSION([5.005_03], [],
   [AC_MSG_ERROR([perl version 5.005_03 or higher is required])])

AC_MSG_NOTICE([verifying build environment ])

# Find RPM, if we have it; this variable is only used for querying the
# release of an RPM-based system
AC_PATH_PROG(ac_rpmquery,rpm,no,[$PATH])

if test "x$ac_rpmquery" == "xno" ; then
   RPMQUERY=/bin/false
else
   RPMQUERY=$ac_rpmquery
fi

# first, if we notice we're being run at UW-Madison CS on a
# CSL-supported machine, and the PATH doesn't include
# /p/condor/workspaces/build/bin before /s/std/bin, print an error
# message and exit, since there's no way we're going to find the first
# versions of everything we need, and it's easier to just tell the
# user right at the start what they need to do to fix it, instead of
# having the rest of the tests in here fail...
my_domain=`echo $ac_hostname | $CUT -f2- -d.`
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


############################################################
# Figure out what platform we're being run on
############################################################

# on Darwin on intel, uname will *always* report i386 when in reality it may
# be an x86_64 machine. Here we determine if sysctl is available, and then
# later we'll use it to figure out the actual platform.
AC_PATH_PROG(ac_sysctl,sysctl,no,[$PATH])
if test "x$ac_sysctl" == "xno" ; then
   SYSCTL=/bin/false
else
   STSCTL=$ac_sysctl
fi

AC_MSG_NOTICE([determining host platform])
# these three are just used by configure to specify the settings we
# actually care about, described below
arch=`uname -m`
osrelease=`uname -r`
osversion=`uname -v`

# this is what ends up as "Architecture" in the Imakefiles, and is
# used in a number of the IS_* #define's too.  luckily, we can use the
# same thing in both places without confusion...
_cv_arch="UNKNOWN_ARCH"

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

# Sometimes, I need to know the specific kernel revision. This is mostly
# needed under Linux, but I'll make every OS produce this identification.
_cv_kernel_rev="XXX"

os=`uname -s`

if test "$os" = "HP-UX"; then
  _cv_op_sys="HPUX"
  _cv_opsys="HPUX"
  if test "$osrelease" = "B.10.20" ; then
    _cv_opsys_vers="HPUX10"
    _cv_sysname="hp_ux102"
  elif test "$osrelease" = "B.11.11" ; then
    _cv_opsys_vers="HPUX11"
    _cv_sysname="hp_ux110"
  elif test "$osrelease" = "B.11.31" ; then
    _cv_opsys_vers="HPUX11"
    _cv_sysname="hp_ux113"
  fi

  hpux_arch=`uname -m`
  if test "x$hpux_arch" = "xia64" ; then
    _cv_arch="IA64"
  else
    _cv_arch="HPPA"
  fi

  _cv_kernel_rev=`uname -r | sed -e 's#^B\.##'`;

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
  elif test $arch = "ppc" -o $arch = "ppc64" ; then
    _cv_arch="CONDOR_PPC"
  elif test $arch = "x86_64" ; then
    _cv_arch="X86_64"
  elif test $arch = "mips64" ; then
    _cv_arch="MIPS64"
  fi

  # next, figure out what linux distribution this is
  if test -f "/etc/tao-release" ; then
    _tao_vers=`cat /etc/tao-release|$PERL -ne '/.* (.*) \(.*\)/;print $1;'`
    case "$_tao_vers" in
     "1" )
       _cv_linux_distro="TAO1"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to Tao version $_tao_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor])
       _cv_linux_distro="TAO_UNKNOWN"
       ;;
    esac
  elif test -f "/etc/yellowdog-release" ; then
    _yd_vers=`cat /etc/yellowdog-release|$PERL -ne '/.* (.*) \(.*\)/;print $1;'`
    case "$_yd_vers" in
     "3.0" )
       _cv_linux_distro="YD30"
       ;;
     "5.0" )
       _cv_linux_distro="YD50"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to yellowdog version $_yd_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor])
       _cv_linux_distro="YD_UNKNOWN"
       ;;
    esac
  elif test -f "/etc/gentoo-release" ; then
    _gentoo_vers=`cat /etc/gentoo-release|$PERL -ne '/Gentoo Base System release (.*)/;print $1;'`
    case "$_gentoo_vers" in
     "1.12.11.1" )
       _cv_linux_distro="GENTOO1.12.11.1"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to gentoo version $_gentoo_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor])
       _cv_linux_distro="GENTOO_UNKNOWN"
       ;;
    esac
  elif test -f "/etc/fedora-release"  ; then
    # do this BEFORE checking for redhat-release, because
    # redhat-release is a symlink to fedora-release on F systems
    _fedora_rel=`cat /etc/fedora-release|$PERL -ne '/.* (.*) \((.*)\)/;print $2;'`
    case "$_fedora_rel" in
     # Handle FC cases explicitly
     "Yarrow" )
        _cv_linux_distro="FC1"
	;;
     "Tettnang" )
        _cv_linux_distro="FC2"
	;;
     "Heidelberg" )
        _cv_linux_distro="FC3"
	;;
     "Stentz" )
        _cv_linux_distro="FC4"
	;;
     "Bordeaux" )
        _cv_linux_distro="FC5"
	;;
     "Zod" )
        _cv_linux_distro="FC6"
	;;
     # Handle post-FC releases more robustly
     * )
        _cv_linux_distro=F`cat /etc/fedora-release|$PERL -ne '/.* (.*) \((.*)\)/;print $1;'`
	;;
    esac
  elif test -f "/etc/redhat-release" ; then
    # Non-Fedora Red Hat or Red Hat derivative -- either RHEL or CentOS
    
    if (rpm -q centos-release 2>&1 > /dev/null) ; then
       # CentOS
       _cv_linux_distro=`$RPMQUERY -q --queryformat "CENTOS%{VERSION}%{RELEASE}" centos-release | tr -d .`
    else
       # Red Hat Linux or RHEL; handle RHL first before presuming RHEL

       if (grep -q "Red Hat Linux" /etc/redhat-release) ; then
         _rh_vers=`cat /etc/redhat-release|$PERL -ne '/.* (.*) \(.*\)/;print $1;'`
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
       	     AC_MSG_WARN([Condor is not ported to Red Hat Linux version $_rh_vers])
	     AC_MSG_WARN([You may have trouble building and/or packaging Condor])
 	     _cv_linux_distro="RHL_UNKNOWN"
             ;;
          esac
       elif (grep -q "Red Hat Enterprise Linux" /etc/redhat-release) ; then
         # RHEL
	 _cv_linux_distro=RHEL`cat /etc/redhat-release|$PERL -ne '/.* (.*) \(.*\)/;print $1;' | $AWK -F. '{printf "%s", $1}'`
       else
          $_rh_release=`cat /etc/redhat-release`
          AC_MSG_WARN([You may have trouble building and/or packaging Condor on $_rh_release])
          _cv_linux_distro=`$RPMQUERY -q --queryformat "RH_UNK_%{VERSION}%{RELEASE}" redhat-release | tr -d .`
       fi
    fi
  elif test -f "/etc/SuSE-release" ; then
    _suse_vers=`cat /etc/SuSE-release | grep VERSION | $AWK '{print $3}'`
    case "$_suse_vers" in
     "8.1" )
       _cv_linux_distro="SLES81"
       ;;
     "9" )
       _cv_linux_distro="SLES9"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to SuSE version $_suse_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor])
       _cv_linux_distro="SuSE_UNKNOWN"
       ;;
    esac
  elif test -f "/etc/debian_version" ; then
    _deb_vers=`cat /etc/debian_version`;
    case "$_deb_vers" in
     "4.0" )
       _cv_linux_distro="DEBIAN40"
       ;;
     5.0* )
       _cv_linux_distro="DEBIAN50"
       ;;
     * )
       AC_MSG_WARN([Condor is not yet ported to Debian version $_suse_vers])
       AC_MSG_WARN([You may have trouble building and/or packaging Condor])
       _cv_linux_distro="DEBIAN_UNKNOWN"
       ;;
	 esac
  else
    AC_MSG_WARN([Unrecognised Linux distribution])
    AC_MSG_WARN([You may have trouble building and/or packaging Condor])
    _cv_linux_distro="UNKNOWN"
  fi
  # this perl 1-liner takes all space-delimited input, sticks it
  # together with '_' marks, and lower-cases the whole thing
  _cv_sysname="`echo $_cv_arch $_cv_linux_distro | $PERL -ane 'print lc join('_',@F)'`"
  _cv_opsys_vers="LINUX_$_cv_linux_distro"

  # now figure out the kernel revision. Under linux this will have
  # the form: major.minor.patch
  _cv_kernel_rev=`uname -r | sed -e 's#^\\([0-9]\\+\\.[0-9]\\+\\.[0-9]\\+\\).*#\\1#'`;

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

  # now figure out the kernel revision. Under solaris this will have
  # the form: major.minor
  _cv_kernel_rev=`uname -r | sed -e 's#^\\([0-9]\\+\\.[0-9]\\+\\).*#\\1#'`;

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

  # now figure out the kernel revision. Under aix this will have
  # the form: major.minor
  _cv_kernel_rev_major=`uname -v`
  _cv_kernel_rev_minor=`uname -r`
  _cv_kernel_rev="$_cv_kernel_rev_major.$_cv_kernel_rev_minor"

# --------------------------------------------
# Darwin
# --------------------------------------------
elif test $os = "Darwin" ; then
	##
	## Figure out the architecture
	##
	if test "$arch" = "i386"; then
		# XXX This test is commented out because the externals aren't
		# smart enough to figure out what platform they are on and Condor
		# will fail to compile. So for now we lie and say INTEL. This
		# could cause problems if a submit machine is x86_64 and the execute
		# machines are x86 since both will report as INTEL.
		# ok, test to see if the hw.optional.x86_64 key is present and 1, 
		# if so, then this machine is actually an x86_64 machine, not an
		# i386 machine.
		#`$SYSCTL hw.optional.x86_64 2>&1 | $GREP '1\$' > /dev/null 2>&1`
		#if test "x$?" = "x1"; then
		#	_cv_arch="X86_64"
		#	_cv_sysname="x86_64_macosx"
		#else
			_cv_arch="I386"
			_cv_sysname="i386_macosx"
		#fi	
	elif test "$arch" = "Power Macintosh"; then
		_cv_arch="PPC"
		_cv_sysname="ppc_macosx"
	else
		AC_MSG_WARN([Unrecognised Darwin Architecture])
		AC_MSG_WARN([You may have trouble building and/or packaging Condor])
		_cv_sysname="unknown_macosx"
	fi
	_cv_op_sys="Darwin"
	_cv_opsys="DARWIN"

	AC_PATH_PROG([SW_VERS],[sw_vers],[no],[$PATH])
	if test $SW_VERS = "no" ; then
    	_cv_opsys_vers="OSX_UNKNOWN"
	else
		_cv_osx_vers=`$SW_VERS | $GREP ^ProductVersion | $PERL -pe 's/^ProductVersion\:\s+(\d+)\.(\d+).*$/$1_$2/'`
		_cv_opsys_vers="OSX_$_cv_osx_vers"
	fi

	# append the product version to the sysname, so we can differentiate
	# between different ports
	tmp=`echo $_cv_osx_vers | $PERL -pe 's/_//g'`
	_cv_sysname="${_cv_sysname}${tmp}"

	# now figure out the kernel revision. Under macosx this will have
	# the form: major.minor.patch
	_cv_kernel_rev=`uname -r | sed -e 's#^\\([0-9]\\+\\.[0-9]\\+\\.[0-9]\\+\\).*#\\1#'`;

# --------------------------------------------
# FreeBSD
# --------------------------------------------
elif test $os = "FreeBSD" ; then
   ##
   ## Figure out the architecture
   ##
   if test $arch = "i686" -o $arch = "i586" -o $arch = "i486" -o $arch = "i386" ; then
      _cv_arch="I386"
   elif test $arch = "alpha" ; then
      _cv_arch="ALPHA"
   elif test $arch = "ia64" ; then
      _cv_arch="IA64"
   elif test $arch = "ppc" -o $arch = "ppc64" ; then
      _cv_arch="CONDOR_PPC"
   ##
   ## Globus does not work on 64-bit
   ## We will use the i386 build until this gets fixed (hopefully not by me!)
   ## Andy - 01/25/2008
   ##
   elif test $arch = "amd64" -o $arch = "x86_64" ; then
      _cv_arch="I386"
      #_cv_arch="X86_64"
   fi

   ##
   ## Now figure out the OS Version
   ## Format:  MAJOR.MINOR-TAG
   ## Example: 6.0-RELEASE
   ##
   _cv_opsys_vers=`uname -r | sed -e 's#\([0-9]*\).*#\1#'`
   _cv_kernel_rev=`uname -r | sed -e 's#[0-9]*\.\([0-9]*\).*#\1#'`

   _cv_op_sys=CONDOR_FREEBSD
   _cv_opsys=${_cv_op_sys}
   _cv_sysname=`echo ${_cv_arch}_freebsd${_cv_opsys_vers} | $PERL -ane 'print lc'`

   ##
   ## Need to make _cv_opsys_vers like 'FREEBSD#'
   ##
   _cv_opsys_vers="FREEBSD${_cv_opsys_vers}"

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

AC_MSG_CHECKING([kernel revision])
AC_MSG_RESULT([$_cv_kernel_rev])
AC_SUBST(_cv_kernel_rev,$_cv_kernel_rev)

# check out the libc
if test "$_cv_opsys" = "LINUX" ; then
  AC_MSG_CHECKING([exact libc version])
  for _libc in "/lib/libc.so.6" "/lib/libc.so.6.1" "/lib64/libc.so.6" ; do
    if test -f "$_libc" && test -L "$_libc" ; then
      _libc_ver=`echo $_libc|$PERL -nle '$f=readlink;$f=~/libc-(.*)\.so/;print $1;'`
      break;
    elif test -f "$_libc" ; then
      # ugh!  it's not a symlink to a file named with the version, so
      # we try an alternative method.  this is mostly a hack based on
      # crude experimentation.  we run nm on the libc.so, search for
      # "A" symbols with "GLIBC_#.#.#" in them, and grab the *second to*
      # last one we find.  based on testing this method on some
      # versions of glibc where the version was in the filename, this
      # seems to give us the closest match to reality.  it'll
      # certainly work to give us the major + minor number, which is
      # mostly all we care about at this point...
      _libc_ver=`nm $_libc|$PERL -e 'while(<>){if(/.*A GLIBC_(\d)\.(\d)\.(\d)/){$s=$v;$v="$1.$2.$3"}}print "$s\n";'`
      break;
    fi
  done
  if test "x$_libc_ver" = "x" ; then
    AC_MSG_RESULT([ERROR])
    AC_MSG_ERROR([Can not determine libc version on this machine])
  fi
  AC_MSG_RESULT([GLIBC $_libc_ver])
  _cv_libc_kind="GLIBC"
  _cv_libc_major=`echo $_libc_ver | $CUT -f1 -d.`
  _cv_libc_minor=`echo $_libc_ver | $CUT -f2 -d.`
  _cv_libc_patch=`echo $_libc_ver | $CUT -f3 -d.`

  _cv_libc_full=`echo $_cv_libc_kind $_cv_libc_major $_cv_libc_minor $_cv_libc_patch|$PERL -ane 'print "#define IS_".join('_',@F)." YES";'`
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

AC_ARG_WITH(buildid,
  [AS_HELP_STRING([--with-buildid],
     [Add a build identification string to the Condor Version String])],
  [BUILDID=$withval],
  [BUILDID=no])

AC_ARG_ENABLE(bgd-support,
  [AS_HELP_STRING([--enable-bgd-support],
     [Enable support for HTC mode on the BlueGene machine.])],
  [],
  [enable_bgd_support=no])

# disabled by default for now, until I'm sure NMI has the required tools
AC_ARG_ENABLE(checksum-md5,
  [AS_HELP_STRING([--enable-checksum-md5],
     [Enable production and validation of MD5 checksums for released packages.])],
  [],
  [enable_checksum_md5=no])

# disabled by default for now, until I'm sure NMI has the required tools
AC_ARG_ENABLE(checksum-sha1,
  [AS_HELP_STRING([--enable-checksum-sha1],
     [Enable production and validation of SHA1 checksums for released packages.])],
  [],
  [enable_checksum_sha1=no])

AC_ARG_ENABLE(proper,
  [AS_HELP_STRING([--enable-proper],
     [Force this configure script to work in the proper (non-Condor) way (default: disabled)])],
  [],
  [enable_proper=no])

AC_ARG_ENABLE(full-port,
  [AS_HELP_STRING([--enable-full-port],
    [Perform a full build, i.e. Standard Universe included (default: enabled)])],
  [CLIPPED=$(test $enableval == yes && echo no || echo yes)],
  [CLIPPED=default])

AC_ARG_ENABLE(gcc-version-check,
  [AS_HELP_STRING([--disable-gcc-version-check],
     [Disable the gcc version check])],
  [],
  [enable_gcc_version_check=yes])

AC_ARG_ENABLE(glibc-version-check,
  [AS_HELP_STRING([--disable-glibc-version-check],
     [Disable the glibc version check])],
  [],
  [enable_glibc_version_check=yes])

AC_ARG_ENABLE(soft-is-hard,
  [AS_HELP_STRING([--enable-soft-is-hard],
     [Force all SOFT requirements into HARD requirements (default: disabled)])],
  [],
  [enable_soft_is_hard=no])

# if we want a buildid, jam it into CPPFLAGS
AC_MSG_CHECKING(for a build id to use)
if test "x$BUILDID" != "xno"; then
	CPPFLAGS="$CPPFLAGS -DBUILDID=$BUILDID"
	AC_MSG_RESULT($BUILDID)
else
	AC_MSG_RESULT(none specified)
fi

# If we are being proper we don't want to look for "externals"
AS_IF([test "x$enable_proper" != xyes],
[
ac_cv_has_externals=YES
ac_cv_externals_src="$_root_dir/externals"
ac_cv_externals_build="$ac_cv_externals_src"
CONDOR_VERIFY_EXTERNALS_DIR([$ac_cv_externals_src],
  [Can't find externals directory, should be in $_root_dir])
AC_ARG_WITH(externals,
 AC_HELP_STRING([--with-externals=DIR],
   [Directory in which to build and install external programs needed for building Condor (default is to build in ../externals)]),
 [if test "$withval" != no; then
    if test ! -d "$withval" || test ! -w "$withval"; then
      AC_MSG_ERROR([directory specified with --with-externals ($withval) is not valid])
    fi
    ac_cv_externals_build="$withval"
  else
    AC_MSG_WARN([By not using --with-externals, most of Condor will not build])
    ac_cv_has_externals=NO
  fi
 ],
 [
  # nothing specified, use the UW-Madison CS cache if present, else assume
  # ../externals. This makes it possible to just run "./configure" most 
  # of the time.
  _uw_cs_externals="/p/condor/workspaces/externals"
  if test -d "$_uw_cs_externals" ; then
    ac_cv_externals_build="$_uw_cs_externals"
  fi
 ]
)
AC_SUBST(has_externals,$ac_cv_has_externals)
AC_SUBST(ext_src,$ac_cv_externals_src)
AC_SUBST(ext_build,$ac_cv_externals_build)
],
[
AC_SUBST(has_externals,NO)
])

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

AC_ARG_ENABLE(job-hooks,
  [AS_HELP_STRING([--enable-job-hooks],
     [Support for invoking hooks throughout the workflow of a job])],
  [JOB_HOOKS=$enableval],
  [JOB_HOOKS=yes])
if test $JOB_HOOKS != no; then
  AC_DEFINE([HAVE_JOB_HOOKS], [1], [Define to 1 to support invoking hooks throughout the workflow of a job])
fi

AC_ARG_ENABLE(hibernation,
  [AS_HELP_STRING([--enable-hibernation],
     [Support for Condor-controlled hibernation])],
  [HIBERNATION=$enableval],
  [HIBERNATION=yes])
if test $HIBERNATION != no; then
  AC_DEFINE([HAVE_HIBERNATION], [1], [Define to 1 to support Condor-controlled hibernation])
fi


############################################################
# checks for programs
############################################################

#####################
# compiler detection
#####################

AC_LANG(C)
# We have to set CFLAGS to something otherwise AC_PROG_CC will add -O2
# to it and that breaks parts of the standard universe. Lucky for us
# -g is always safe to have in CFLAGS. The standard universe breakage
# comes from stdio.h, which includes inline versions of functions when
# -O2 is specified, instead of regular symbols. As a result the symbol
# interposition in the standard universe fails to find all the symbols
# it needs to.
CFLAGS="-g $CFLAGS"
AC_PROG_CC
if test $ac_cv_c_compiler_gnu != yes; then
   AC_MSG_ERROR( [GNU CC (gcc) is required] )
fi

CHECK_PATH_PROG(gcc,gcc)


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
GCC=gcc
# "Sometimes the user knows what he is doing" If a user specifies CC
# we don't want configure to use it and Condor to use a hard-coded
# "gcc"
if test -n "$CC"; then
  GCC=$CC
fi
gcc_vers=`$GCC -v 2>&1 |$PERL -ne '{if(/.*gcc version\s*(\S*).*/){ print "$1\n";}}'`

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
 "3.2.3" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="2"
    CompilerPatch="3"
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
 "3.3.3" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="3"
    ;;
 "3.3.4" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="4"
    ;;
 "3.3.5" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="3"
    CompilerPatch="5"
    ;;
 "3.4.2" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="4"
    CompilerPatch="2"
    ;;
 "3.4.3" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="4"
    CompilerPatch="3"
    ;;
 "3.4.4" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="4"
    CompilerPatch="4"
    ;;
 "3.4.6" )
    CompilerKind="GCC"
    CompilerMajor="3"
    CompilerMinor="4"
    CompilerPatch="6"
    ;;
 "4.0.0" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="0"
    CompilerPatch="0"
    ;;
 "4.0.1" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="0"
    CompilerPatch="1"
    ;;
 "4.0.2" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="0"
    CompilerPatch="2"
    ;;
 "4.0.3" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="0"
    CompilerPatch="3"
    ;;
 "4.1.1" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="1"
    CompilerPatch="1"
    ;;
 "4.1.2" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="1"
    CompilerPatch="2"
    ;;
 "4.2.3" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="2"
    CompilerPatch="3"
    ;;
 "4.2.1" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="2"
    CompilerPatch="1"
    ;;
 "4.3.0" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="3"
    CompilerPatch="0"
    ;;
 "4.3.1" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="3"
    CompilerPatch="1"
    ;;
 "4.3.2" )
    CompilerKind="GCC"
    CompilerMajor="4"
    CompilerMinor="3"
    CompilerPatch="2"
    ;;
 * )
    if test "x$enable_gcc_version_check" = "xyes"; then
      AC_MSG_RESULT([ERROR])
      AC_MSG_ERROR([Condor will not compile with gcc version $gcc_vers])
    else
      AC_MSG_RESULT([$gcc_vers (UNSUPPORTED)])
      CompilerKind="UNSUPPORTED"
      CompilerMajor="0"
      CompilerMinor="0"
      CompilerPatch="0"
    fi
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
if test "X$CompilerKind" = "XGCC" && test "X$CompilerMajor" = "X3" -o "X$CompilerMajor" = "X4"; then
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
    elif test $os = "HP-UX" && test "$osversion" = "B.11.31" ; then
      CRT_BEGIN=/usr/ccs/lib/hpux32/crt0.o
    fi
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
CHECK_PATH_PROG([g++],[gpp])


#########
# figure out if we're using GNU ld (and therefore, can use --wrap)
#########
GET_GCC_VALUE([ld used by gcc],[-print-prog-name=ld],[gcc_ld_prog])
_gcc_ld_dir=`AS_DIRNAME(["$_cv_gcc_ld_prog"])`
if test "$_gcc_ld_dir" = "." ; then
  # gcc doesn't have a full path, find out what we're really using
  AC_PATH_PROG([LD],[ld],[no],[$PATH])
else
  LD=$_cv_gcc_ld_prog
fi

CHECK_PROG_IS_GNU([$LD],[ld])
if test $_cv_ld_is_gnu = yes; then
   AC_DEFINE(HAVE_GNU_LD,1,[are we using the GNU linker])
fi


#########
# figure out what linker flags put us in static-link mode
#########
AC_MSG_NOTICE([checking what gcc flag forces $_cv_gcc_ld_prog into static mode])
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
  #####
  for arg in "-Bstatic" "-B,static" "-a,archive" ; do
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

  # Now test for the -Wl,--hash-type=both flag
  AC_MSG_CHECKING([hash-style flag])
  echo "main() { return 0; }" > link_test.c
  _test_output=`gcc -Wl,--hash-style=both -o link_test link_test.c > /dev/null 2>&1`
  _test_status=$?
  if test $_test_status -eq 0 && test "x$_test_output" = "x"; then
    gcc_hash_style_flag=-Wl,--hash-style=both
    AC_MSG_RESULT([yes])
  else
    AC_MSG_RESULT([no])
  fi
  rm -f link_test link_test.c 
  AC_SUBST([gcc_hash_style_flag],[$gcc_hash_style_flag])

fi


#########
# determine if gcc has a collection of particular warning flags we desire.
#########

#
# These options are shared between gcc and g++
#

AC_SUBST([DEBUG_FLAGS], $debug_flags)

# The standard warnings
C_CXX_OPTION(gcc, -Wall, comp_option_Wall, [])

# Extra options in later compilers, -W and -Wextra are the same
C_CXX_OPTION(gcc, -W, comp_option_W, [])
C_CXX_OPTION(gcc, -Wextra, comp_option_Wextra, [])

# Don't use == to compare floats
C_CXX_OPTION(gcc, -Wfloat-equal, comp_option_Wfloat_equal, [])

# Warn if there is a shadow declaration, like a local with the same name as
# a global
C_CXX_OPTION(gcc, -Wshadow, comp_option_Wshadow, [])

# Warn if code can't be executed.
#
# Note: Code blocks marked by this warning flag should be carefully checked
# since they may be able to be executed in ways the compiler doesn't see, like
# via setjmp, or other methods.
# XXX This warning looks like it gives far too many false positive, so it
# is getting commented out. If you'd like to renable this warning flag, please
# make sure to add @comp_option_Wunreachable_code@ to COMP_DEBUG_FLAGS
#C_CXX_OPTION(gcc, -Wunreachable-code, comp_option_Wunreachable_code, [])

# Warn if non-comment after endif labels
C_CXX_OPTION(gcc, -Wendif-labels, comp_option_Wendif_labels, [])

# Warn on arithmatic with void*, cause you don't know the size...
C_CXX_OPTION(gcc, -Wpointer-arith, comp_option_Wpointer_arith, [])

# Warn whenever a type qualifier (like const) is cast away.
C_CXX_OPTION(gcc, -Wcast-qual, comp_option_Wcast_qual, [])

# Warn whenever a pointer is cast such that the required alignment of the target
# is increased.
C_CXX_OPTION(gcc, -Wcast-align, comp_option_Wcast_align, [])

# Warn whenever a static object on the heap is larger than 256 bytes.
# This causes ICE when using gcc 2.96 on x6 rh7.2, and various staff are
# in disagreement about the validity of it, so we'll leave it commented out
# for now. Please ensure to add back in the @comp_option_Wvolatile_register_var@
# back into COMP_DEBUG_FLAGS if you want to enable it again.
#C_CXX_OPTION(gcc, -Wlarger-than-92160, comp_option_Wlarger_than_92160, [])

# Warn if a register variable is declared volatile. The optimizer doesn't
# inhibit all transformations of register variables when they are declared
# volatile, so an error could be introduced.
C_CXX_OPTION(gcc, -Wvolatile-register-var, comp_option_Wvolatile_register_var, [])

# Warn if writing to a const char* string constant (which lives in read-only
# memory space).
#
# Commented out since it is a bit spurious in its errors. It makes an
# assumption that we've been careful about using const everywhere when
# declaring write-only strings. Since I'm not confident we've been doing
# that, I took it out. Make sure to add the @comp_option_Wwrite_strings@
# variable back into COMP_DEBUG_FLAGS if you want to enable it again.
#C_CXX_OPTION(gcc, -Wwrite-strings, comp_option_Wwrite_strings, [])

####
# These are C only and treated differently in configure.cf.in
####
# Warn if an extern declaration is encountered in a C function.
C_CXX_OPTION(gcc, -Wnested-externs, comp_option_Wnested_externs, [])

# Warn if a function is declared or defined without argument types
C_CXX_OPTION(gcc, -Wstrict-prototypes, comp_option_Wstrict_prototypes, [])

# Warn if a global function is defined without a previous protoype declaration
C_CXX_OPTION(gcc, -Wmissing-prototypes, comp_option_Wmissing_prototypes, [])

#########
# See if the wonderful stack protection feature is available
#########
# Put guards around arrays on the stack, and check them on function exit.
C_CXX_OPTION(gcc, -fstack-protector, comp_option_stack_protector, [])
# sometimes we need to turn them off too, this overrides the above if both
# appear on the command line.
C_CXX_OPTION(gcc, -fno-stack-protector, comp_option_no_stack_protector, [])

#########
# determine if gcc has -rdynamic to keep backtrace symbols for debugging
# stripped executables.
#########
C_CXX_OPTION(gcc, -rdynamic, gcc_rdynamic_flag, [])

#########
# determine if gcc has -fPIC
#########
AC_MSG_CHECKING([if gcc supports -fPIC])

echo "int main(void) { return 0; }" > fpic_test.c
_test_output=`gcc -fPIC fpic_test.c -o fpic_test 2>&1`
_test_status=$?
# AIX and HPUX have idiotic problems, so turn them off for now
if test $_test_status -eq 0 -a "x$_test_output" = "x" -a \( "x$os" != "xAIX" -a "x$os" != "xHP-UX" \); then
  cc_pic_flags="-fPIC -DPIC"
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_CC_PIC_FLAG], [1], [does gcc support the -fPIC flag])
else
  cc_pic_flags=""
  AC_MSG_RESULT([no])
fi
rm -f fpic_test.c fpic_test
AC_SUBST([cc_pic_flags],[$cc_pic_flags])

#########
# determine if gcc has -shared
#########
AC_MSG_CHECKING([if gcc supports -shared])

echo "int bar(void) { return 0; }" > fpic_test.c
_test_output=`gcc -shared -fPIC -DPIC fpic_test.c -o libfpic_test.so 2>&1`
_test_status=$?
# AIX and HPUX have idiotic problems, so turn them off for now
if test $_test_status -eq 0 -a "x$_test_output" = "x" -a \( "x$os" != "xAIX" -a "x$os" != "xHP-UX" \); then
  AC_MSG_RESULT([yes])
  cc_shared_flags="-shared"
  AC_DEFINE([HAVE_CC_SHARED_FLAG], [1], [does gcc support the -shared flag])
else
  cc_shared_flags=""
  AC_MSG_RESULT([no])
fi
rm -f fpic_test.c libfpic_test.so
AC_SUBST([cc_shared_flags],[$cc_shared_flags])

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

# And check for gfortan as well.
AC_PATH_PROG([_cv_gnu_gfortran],[gfortran],[no],[$PATH])
if test "$_cv_gnu_gfortran" = "no" ; then
  _cv_has_gnu_gfortran="NO"
else
  _cv_has_gnu_gfortran="YES"
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
    _vendor_cc_PATH="/opt/ansic/bin:/opt/CC/bin:/usr/bin:/bin:$PATH"
    _vendor_f_PATH="/opt/fortran/bin:/opt/fortran90/bin:/usr/bin:/bin:$PATH"
  else
    _vendor_cc_PATH="/opt/SUNWspro/bin:/usr/ucb:/usr/bin:/bin:$PATH"
    _vendor_f_PATH=$_vendor_cc_PATH
  fi
  AC_PATH_PROG([_cv_vendor_cc],[cc],[no],[$_vendor_cc_PATH])
  if test "$_cv_vendor_cc" = "no" ; then
    _cv_has_vendor_cc="NO"
  else
    _cv_has_vendor_cc="YES"
  fi

  AC_PATH_PROGS([_cv_vendor_cplus],[CC cxx],[no],[$_vendor_cc_PATH])
  if test "$_cv_vendor_cplus" = "no" ; then
    _cv_has_vendor_cplus="NO"
  else
    _cv_has_vendor_cplus="YES"
    _cv_vendor_cplus_name=`basename $_cv_vendor_cplus`
      AC_SUBST([_cv_vendor_cplus_name],[$_cv_vendor_cplus_name])
  fi

  AC_PATH_PROG([_cv_vendor_f77],[f77],[no],[$_vendor_f_PATH])
  if test "$_cv_vendor_f77" = "no" ; then
    _cv_has_vendor_f77="NO"
  else
    _cv_has_vendor_f77="YES"
  fi

  AC_PATH_PROG([_cv_vendor_f90],[f90],[no],[$_vendor_f_PATH])
  if test "$_cv_vendor_f90" = "no" ; then
    _cv_has_vendor_f90="NO"
  else
    _cv_has_vendor_f90="YES"
  fi
fi
AC_SUBST([has_gnu_g77],[$_cv_has_gnu_g77])
AC_SUBST([has_gnu_gfortran],[$_cv_has_gnu_gfortran])
AC_SUBST([has_vendor_cc],[$_cv_has_vendor_cc])
AC_SUBST([has_vendor_cplus],[$_cv_has_vendor_cplus])
AC_SUBST([has_vendor_f77],[$_cv_has_vendor_f77])
AC_SUBST([has_vendor_f90],[$_cv_has_vendor_f90])


###############################################################
# Check for functionality availability on different platforms #
###############################################################

###
# Check for the availability of full ports
#
# CLIPPED is available here as "yes", "no" or "default"
#
# If CLIPPED is default it means the user did not specify if she wants
# a clipped build or not. In such a case we check to see if a full
# port is available and if so try to build a full port.
###

# NOTES:
#
# Due to two problems we turn off the full port for Debian:
#
#  1. -fstack-protect has a support library that our glibc doesn't have and
#      we need to copy its functionality for standard universe to link.
#  2. C++ exception handling in stdc++ assumes that the glibc is compiled
#     with TLS, but ours isn't and doesn't compile out of the box with
#     TLS.
	
AC_MSG_CHECKING([for clipped port default])
case "$_cv_opsys-$_cv_opsys_vers-$_cv_arch-$_cv_linux_distro-$_cv_libc_major-$_cv_libc_minor" in
  *-AIX5-* | \
  *-DUX5-* | \
  DARWIN-* | \
  CONDOR_FREEBSD-* | \
  *-HPUX11-* | \
  LINUX-*-ALPHA-* | \
  LINUX-*-*-DEBIAN40-* | \
  LINUX-*-IA64-* | \
  SOLARIS-* | \
  LINUX-*-CONDOR_PPC-* ) _cv_clipped_default=yes ;;
  * ) _cv_clipped_default=no ;;
esac
AC_MSG_RESULT([$_cv_clipped_default])

AC_MSG_CHECKING([for clipped port])
if test "x$CLIPPED" = "xdefault"; then
  CLIPPED=$_cv_clipped_default
fi
AC_MSG_RESULT([$CLIPPED])

# Exposed in configure.cf.in for Imake.rules's CLINK*
AC_SUBST([_cv_is_clipped], [$CLIPPED])

if test "x$CLIPPED" = "xyes"; then
  AC_DEFINE([IS_CLIPPED], [1], [Define if doing a clipped build])
fi

# Remote Syscalls

AC_MSG_CHECKING([if DOES_REMOTE_SYSCALLS is supported])
case "$_cv_opsys" in
  HPUX | LINUX | SOLARIS )
    _cv_does_remote_syscalls=yes
  ;;
  * )
    _cv_does_remote_syscalls=no
  ;;
esac
AC_MSG_RESULT([$_cv_does_remote_syscalls])
AS_IF([test "x$CLIPPED" != "xyes" -a "x$_cv_does_remote_syscalls" = xyes],
  AC_DEFINE([DOES_REMOTE_SYSCALLS], [1], [Define if we can do remote syscalls]))

# Checkpointing

AC_MSG_CHECKING([if DOES_CHECKPOINTING is supported])
case "$_cv_opsys" in
  HPUX | LINUX | SOLARIS )
    _cv_does_checkpointing=yes
  ;;
  * )
    _cv_does_checkpointing=no
  ;;
esac
AC_MSG_RESULT([$_cv_does_checkpointing])
AS_IF([test "x$CLIPPED" != "xyes" -a "x$_cv_does_checkpointing" = xyes],
  AC_DEFINE([DOES_CHECKPOINTING], [1], [Define if we can do checkpointing]))

# Compressed checkpoints

AC_MSG_CHECKING([if DOES_COMPRESS_CKPT is supported])
# Turned off on LINUX-X86_64: due to thaineryconcerning void* and int, don't compile this in for now...
case "$_cv_opsys-$_cv_arch" in
  LINUX-X86_64 )
    _cv_does_compress_ckpt=no
  ;;
  * )
    _cv_does_compress_ckpt=yes
  ;;
esac
AC_MSG_RESULT([$_cv_does_compress_ckpt])
AS_IF([test "x$CLIPPED" != "xyes" -a "x$_cv_does_compress_ckpt" = xyes],
  AC_DEFINE([DOES_COMPRESS_CKPT], [1], [Define if we can compress checkpoints]))

AC_MSG_CHECKING([if DOES_SAVE_SIGSTATE])
case "$_cv_opsys" in
  AIX | HPUX | LINUX | SOLARIS )
    _cv_does_save_sigstate=yes
  ;;
  * )
    _cv_does_save_sigstate=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_does_save_sigstate])
AS_IF([test "x$_cv_does_save_sigstate" = xyes],
  AC_DEFINE([DOES_SAVE_SIGSTATE], [1], [Define if we save sigstate]))

AC_MSG_CHECKING([if HAS_RANLIB_TOUCH])
case "$_cv_opsys" in
  AIX | HPUX | SOLARIS )
    _cv_has_ranlib_touch=yes
  ;;
  LINUX )
    _cv_has_ranlib_touch=no
  ;;
  * )
    _cv_has_ranlib_touch=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_has_ranlib_touch])
AS_IF([test "x$_cv_has_ranlib_touch" = xyes],
  AC_DEFINE([HAS_RANLIB_TOUCH], [1], [Define if we want HAS_RANLIB_TOUCH]))

AC_MSG_CHECKING([if HAS_CP_PRESERVE])
case "$_cv_opsys" in
  AIX | HPUX | SOLARIS )
    _cv_has_cp_preserve=yes
  ;;
  LINUX )
    _cv_has_cp_preserve=no
  ;;
  * )
    _cv_has_cp_preserve=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_has_cp_preserve])
AS_IF([test "x$_cv_has_cp_preserve" = xyes],
  AC_DEFINE([HAS_CP_PRESERVE], [1], [Define if we want HAS_CP_PRESERVE]))

AC_MSG_CHECKING([if HAS_PURIFY])
case "$_cv_opsys" in
  SOLARIS )
    _cv_has_purify=yes
  ;;
  * )
    _cv_has_purify=no
  ;;
esac
AC_MSG_RESULT([$_cv_has_purify])
AS_IF([test "x$_cv_has_purify" = xyes],
  AC_DEFINE([HAS_PURIFY], [1], [Define if we want HAS_PURIFY]))

AC_MSG_CHECKING([if HAS_AR_S_OPTION])
case "$_cv_opsys" in
  AIX | HPUX | LINUX | SOLARIS )
    _cv_has_ar_s_option=yes
  ;;
  * )
    _cv_has_ar_s_option=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_has_ar_s_option])
AS_IF([test "x$_cv_has_ar_s_option" = xyes],
  AC_DEFINE([HAS_AR_S_OPTION], [1], [Define if ar has the s option]))

AC_MSG_CHECKING([if FORTRAN_HAS_RECURSION])
case "$_cv_opsys" in
  SOLARIS )
    _cv_fortran_has_recursion=yes
  ;;
  * )
    _cv_fortran_has_recursion=no
  ;;
esac
AC_MSG_RESULT([$_cv_fortran_has_recursion])
AS_IF([test "x$_cv_fortran_has_recursion" = xyes],
  AC_DEFINE([FORTRAN_HAS_RECURSION], [1], [Define if FORTRAN_HAS_RECURSION]))

AC_MSG_CHECKING([if NEEDS_64BIT_SYSCALLS])
case "$_cv_opsys" in
  SOLARIS )
    _cv_needs_64bit_syscalls=yes
  ;;
  AIX | HPUX )
    _cv_needs_64bit_syscalls=no
  ;;
  * )
    _cv_needs_64bit_syscalls=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_needs_64bit_syscalls])
AS_IF([test "x$_cv_needs_64bit_syscalls" = xyes],
  AC_DEFINE([NEEDS_64BIT_SYSCALLS], [1], [Define if NEEDS_64BIT_SYSCALLS]))

AC_MSG_CHECKING([if NEEDS_64BIT_STRUCTS])
case "$_cv_opsys" in
  AIX | HPUX | SOLARIS )
    _cv_needs_64bit_structs=yes
  ;;
  * )
    _cv_needs_64bit_structs=unknown
  ;;
esac
AC_MSG_RESULT([$_cv_needs_64bit_structs])
AS_IF([test "x$_cv_needs_64bit_structs" = xyes],
  AC_DEFINE([NEEDS_64BIT_STRUCTS], [1], [Define if NEEDS_64BIT_STRUCTS]))

AC_MSG_CHECKING([if HAS_FLOCK])
case "$_cv_opsys" in
  SOLARIS )
    _cv_has_flock=no
  ;;
  * )
    _cv_has_flock=yes
  ;;
esac
AC_MSG_RESULT([$_cv_has_flock])
AS_IF([test "x$_cv_has_flock" = xyes],
  AC_DEFINE([HAS_FLOCK], [1], [Define if HAS_FLOCK]))

AC_MSG_CHECKING([if HAS_INET_NTOA])
# inet_nota() with gcc is broken under 64-bit compilation mode, so use ours
if test "x$_cv_opsys_vers" = xHPUX11; then
  _cv_has_inet_ntoa=no
else
  _cv_has_inet_ntoa=yes
fi
AC_MSG_RESULT([$_cv_has_inet_ntoa])
AS_IF([test "x$_cv_has_inet_ntoa" = xyes],
  AC_DEFINE([HAS_INET_NTOA], [1], [Define if HAS_INET_NTOA]))

# HAS_DYNAMIC_USER_JOBS, not sure exactly what this is -matt 26 Nov 07
if test "x$_cv_opsys" = "xSOLARIS"; then
  AC_DEFINE([HAS_DYNAMIC_USER_JOBS], [1], [Used in condor_ckpt/image.C])
fi

# Old SOLARIS2*.cf files contained -DSolaris2*=Solaris2*, so instead
# we AC_DEFINE Solaris2* here
case "$_cv_opsys_vers" in
  SOLARIS28 ) AC_DEFINE([Solaris28], [Solaris28], [Define if on Solaris28]) ;;
  SOLARIS29 ) AC_DEFINE([Solaris29], [Solaris29], [Define if on Solaris29]) ;;
esac

# Old FREEBSD*.cf files contained -DCONDOR_FREEBSD*=CONDOR_FREEBSD*,
# so instead we AC_DEFINE CONDOR_FREEBSD* here
case "$_cv_opsys_vers" in
  FREEBSD4 )
    AC_DEFINE([CONDOR_FREEBSD4], [CONDOR_FREEBSD4], [Define if on FreeBSD4]) ;;
  FREEBSD5 )
    AC_DEFINE([CONDOR_FREEBSD5], [CONDOR_FREEBSD5], [Define if on FreeBSD5]) ;;
  FREEBSD6 )
    AC_DEFINE([CONDOR_FREEBSD6], [CONDOR_FREEBSD6], [Define if on FreeBSD6]) ;;
  FREEBSD7 )
    AC_DEFINE([CONDOR_FREEBSD7], [CONDOR_FREEBSD7], [Define if on FreeBSD7]) ;;
esac

# DARWIN.cf contained -DDarwin_10_*=Darwin_10_*, so instead we
# AC_DEFINE Darwin_10_* here
case "$_cv_opsys_vers" in
  OSX_10_3 ) AC_DEFINE([Darwin_10_3], [Darwin_10_3], [Define if on OS X 10.3]) ;;
  OSX_10_4 ) AC_DEFINE([Darwin_10_4], [Darwin_10_4], [Define if on OS X 10.4]) ;;
esac

# Old ALPHA_OSF1_V*.cf files contained -DDUX=DUX -DDUX*=DUX* -DTRU64,
# so instead we AC_DEFINE them here, TRU64 is just for gsoap
if test "x$_cv_opsys" = xyes; then
  AC_DEFINE([DUX], [DUX], [Define if on DUX])
  AC_DEFINE([TRU64], [DUX], [Define if on TRU64 (DUX)])

  case "$_cv_opsys_vers" in
    DUX5 ) AC_DEFINE([DUX5], [DUX5], [Define if on DUX5]) ;;
  esac
fi

# ArchOptimizeFlag logic used to live in .cf files, mostly
# LINUX_I386.cf, that logic now lives here.
if test "x$_cv_opsys" = xLINUX \
   -a "x$_cv_arch" = xI386 \
   -a "x$CompilerKind" = xGCC; then
  case "$CompilerMajor" in
    4 ) _cv_arch_optimize_flag="-march=i486" ;;
    3 ) _cv_arch_optimize_flag="-mcpu=i486" ;;
    * ) _cv_arch_optimize_flag="-m486" ;;
  esac
  AC_SUBST([_cv_arch_optimize_flag], [$_cv_arch_optimize_flag])
fi

# VendorCTestFlags logic used to live in .cf files, mostly
# HPUX11.cf, that logic now lives here.
if test "x$_cv_opsys_vers" = xHPUX11; then
  _cv_vendor_c_test_flags="-Aa -Ae";
  AC_SUBST([_cv_vendor_c_test_flags], [$_cv_vendor_c_test_flags])
fi

# VendorCPPTestFlags logic used to live in .cf files, mostly
# HPUX11.cf, that logic now lives here.
if test "x$_cv_opsys_vers" = xHPUX11; then
  _cv_vendor_cpp_test_flags="-Aa -ext +eh";
  AC_SUBST([_cv_vendor_cpp_test_flags], [$_cv_vendor_cpp_test_flags])
fi


# PlatformLdFlags used to live in a number of .cf files, their logic
# now exists here
case "$_cv_opsys" in
  # specifically ask for the C++ libraries to be statically linked
  AIX ) _cv_platform_ld_flags="-Wl,-berok -Wl,-bstatic -lstdc++ -Wl,-bdynamic -lcfg -lodm -lcrypt -lc -static-libgcc" ;;
  LINUX | DARWIN ) _cv_platform_ld_flags="-lresolv" ;;
  DUX ) _cv_platform_ld_flags="-lmld" ;;
  HPUX ) # HPUX11 Only...
    if test "x$_cv_opsys_vers" = "xHPUX11" ; then
      _cv_platform_ld_flags="-Wl,+vnocompatwarnings $_cv_lib_std_cpp";
    fi
   ;;
#NOTE: The following definition of PlatformLdFlags assumes that the
#compiler libraries (libgcc and libstdc++) are available _only_ as
#static libraries, so we do not need to do anything special to cause
#them to be statically linked.  We want them to be statically linked
#so people can run condor binaries on machines where gcc has never
#been installed, since this is common for Solaris boxes.
#
#We used to append $(GCC_STATIC_LIBS) to PlatformLdFlags, however, this
#caused libc to be statically linked, and that caused runtime errors on
#binaries compiled on Solaris 9 when run on Solaris 10.  Why was libc
#statically linked you may ask?  Because GCC_STATIC_LIBS was defined
#like this: -Wl,-bstatic -lstdc++ -lgcc.  This turns on static linking
#but does not turn it off.  The compiler appends a bunch of standard
#libs, including -lstdc++, -lgcc, and -lc.  Therefore, libc was being
#statically linked.  We could have turned off static linking with
#-Wl,-bdyanamic, so the libs appended by the compiler would not be
#statically linked.  However, among the libs appended by the compiler
#are the very libs we are trying to link statically, so if they were
#available both statically and dynamically, they would be linked both
#ways.  The version of gcc currently in use on the Solaris build
#machines is 2.95.3, which does not support the option -nodefaultlibs,
#so we can't easily prevent the compiler from doing this.  It does
#support -nostdlibs, but that option also disables the linking of all
#other standard things, including the runtime object files, so it
#becomes a rather messy situation to make sure we link in all the stuff
#we need without getting more than we want.
#
#Therefore, we do not explicitly call for static linking of the
#compiler libs.  Instead, we depend on the compiler being installed
#with _only_ static versions of libgcc and libstdc++.
  SOLARIS ) _cv_platform_ld_flags="-lkstat -lelf -lsocket -lresolv -lnsl -lm -lc" ;;
esac
AC_SUBST([_cv_platform_ld_flags], [$_cv_platform_ld_flags])


# LINUX.cf defined GlibCFlag to be "-DGLIBC=GLIBC -DGLIBCXY=GLIBCXY"
# where X is the _cv_libc_major and Y is _cv_libc_minor. This
# GlibCFlag #define was one that someone would need to create each
# time they wanted to do even a clipped port of Condor. Instead of all
# that, we can set that symbol right here, sensibly.
AS_IF([test "x$_cv_libc_kind" = xGLIBC],
  [_cv_glibc_flag="-DGLIBC=GLIBC -DGLIBC$_cv_libc_major$_cv_libc_minor=GLIBC$_cv_libc_major$_cv_libc_minor"])
AC_SUBST([_cv_glibc_flag], [$_cv_glibc_flag])

# PlatformFlags logic has been moved here from .cf files, primarily
# HPUX11.cf and LINUX.cf.
case "$_cv_opsys" in
  LINUX ) _cv_platform_flags="$_cv_arch_optimize_flag $_cv_glibc_flag" ;;
  HPUX ) # HPUX11 Only...
    if test "x$_cv_opsys_vers" = "xHPUX11" ; then
      _cv_platform_flags="-DHPUX11=HPUX11";
    fi
   ;;
  * ) _cv_platform_flags="" ;;
esac
AC_SUBST([_cv_platform_flags], [$_cv_platform_flags])


# TestPlatformLdFlags logic has been moved here from .cf files,
# primarily SOLARIS.cf and AIX.cf.
case "$_cv_opsys" in
  SOLARIS ) _cv_test_platform_ld_flags="-lnsl -lsocket -lresolv" ;;
  AIX )_cv_test_platform_ld_flags="$_cv_platform_ld_flags" ;;
  * ) _cv_test_platform_ld_flags="" ;;
esac
AC_SUBST([_cv_test_platform_ld_flags], [$_cv_test_platform_ld_flags])


# PurifyPlatformLdFlags logic has been moved here from .cf files,
# primarily SOLARIS.cf.
case "$_cv_opsys" in
  # When we're linking for purify, we don't want all the static junk,
  # either, but we need more libs than just the test suite.  So, we
  # define this here to specify the flags we want to give to the
  # linker when we're building the Condor Daemons under purify.
  SOLARIS ) _cv_purify_platform_ld_flags="-lkstat -lelf -lnsl -lsocket -lresolv" ;;
  * ) _cv_purify_platform_ld_flags="" ;;
esac
AC_SUBST([_cv_purify_platform_ld_flags], [$_cv_purify_platform_ld_flags])


#####################
# everything else
#####################

AC_PATH_PROGS(MAKE,gmake make,no,[$PATH])
CHECK_PROG_IS_GNU([$MAKE], make)
if test $_cv_make_is_gnu = no; then
   AC_MSG_ERROR( [GNU make is required] )
fi

AC_PATH_PROG(ac_patch,patch,no,[$PATH])
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
  $ac_patch < conftest_patch > /dev/null 2>&1
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
CHECK_PATH_PROG([$LEX],[lex])


# We don't want to do the default AC_PROG_YACC, since it selects
# "bison -y" if it can find it, and that doesn't work for the yacc
# input in the Condor build.  So, we just manually check for byacc as
# our first choice, then regular old yacc...
AC_PATH_PROG([ac_byacc],[byacc],[no],[$PATH])
if test $ac_byacc = "no"; then
  AC_PATH_PROG([ac_yacc],[yacc],[no],[$PATH])
  if test $ac_yacc = no; then
    AC_MSG_ERROR( [byacc or yacc is required] )
  else
    YACC=$ac_yacc
  fi
else
  YACC=$ac_byacc
fi
AC_SUBST([YACC],[$YACC])

# ar is used for numerous libraries in Condor, and for extracting
# objects from the system libc.a for the Condor syscall library.
AC_PATH_PROG(ac_ar,ar,no,[$PATH])
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
  if ! test "$os" = "Darwin" ; then
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
# Hack: /usr/bin/strip on Mac OS 10.5 causes the condor_master to die
# inside the plugin initialization code. A message thread in the Boost
# mailing list suggests that running strip without -g or its equivalent
# removes more than just debugging symbols, possibly causing RTTI lookups
# to fail. The appropriate flags on Mac OS are '-S -x'.
if test "$os" = "Darwin" ; then
  STRIP="$STRIP -S -x"
fi
AC_MSG_RESULT([$STRIP])
AC_SUBST([STRIP],[$STRIP])

AC_PATH_PROG(ac_find,find,no,[$PATH])
if test $ac_find = "no"; then
  AC_MSG_WARN( [find not found, debuglink not supported] )
  AC_SUBST(FindCmd,[""])
else
  AC_DEFINE(HAVE_FIND,[1],[Define to 1 if the tool 'find' is available])
  AC_SUBST(FindCmd,$ac_find)
fi

AC_PATH_PROG(ac_objcopy,objcopy,no,[$PATH])
if test $ac_objcopy = "no"; then
  AC_SUBST(ObjcopyCmd,[""])
else
  AC_DEFINE(HAVE_OBJCOPY,[1],[Define to 1 if the tool 'objcopy' is available])
  AC_SUBST(ObjcopyCmd,$ac_objcopy)
fi

# If objcopy is available, then figure out if it can do the debuglink
# feature to allow us seperate debugging symbols and executables on
# unix builds ala windows.
_cv_objcopy_keepdebug_flag=""
_cv_objcopy_stripunneeded_flag=""
_cv_objcopy_debuglink_flag=""
if test ! $ac_objcopy = "no"; then
	AC_MSG_CHECKING([if objcopy supports --add-gnu-debuglink])
	`$ac_objcopy --help 2>&1 | $GREP add-gnu-debuglink > /dev/null 2>&1`
	if test $? = 0 -a ! \( "x$_cv_sysname" = "xhp_ux113" -o "x$_cv_sysname" = "xsun4x_59" \)  ; then
		# If this flag exists, then the other ones I need should exist as well.
		_cv_objcopy_keepdebug_flag="--only-keep-debug"
		_cv_objcopy_stripunneeded_flag="--strip-unneeded"
		_cv_objcopy_debuglink_flag="--add-gnu-debuglink"
		AC_MSG_RESULT([yes])
		AC_DEFINE(HAVE_OBJCOPY_DEBUGLINK,[1],[Define to 1 if objcopy can seperate the debugging symbols from an executable])
	else
		AC_MSG_RESULT([no])
	fi
fi
AC_SUBST(OBJCOPY_KEEPDEBUG_FLAG,$_cv_objcopy_keepdebug_flag)
AC_SUBST(OBJCOPY_STRIPUNNEEDED_FLAG,$_cv_objcopy_stripunneeded_flag)
AC_SUBST(OBJCOPY_DEBUGLINK_FLAG,$_cv_objcopy_debuglink_flag)

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
      if test $WHICH != no; then
        _cv_gnu_tar=`$WHICH $a`;
      else
        _cv_gnu_tar="$a";
      fi
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

# Tar uses an idiotic 10KB default io buffer block size, which makes for
# hideously slow tar operations. This isn't 1975 anymore, so I'm upping it
# to 2MB. This is 4096 * 512 (the default block size) and the size of most
# drive caches these days. This option will
# also make the smallest tarfile which can be made be padded to 2MB. However,
# we compress all of our tar files, so those fluffy zeros will compact nicely.
# I wish tar wasn't so stupid about the read/write block sizes... :(
CHECK_TAR_OPTION(["-b 4096"],[_cv_gnu_tar_blocksize_flag])
if test "$_cv_gnu_tar_blocksize_flag" = "yes"; then
	TarCmd="$TarCmd -b 4096"
fi

AC_SUBST(TarCmd,$TarCmd)

# Check to see if tar supports --exclude
CHECK_TAR_OPTION([--exclude=configure],[_cv_gnu_tar_exclude_flag])
if test "$_cv_gnu_tar_exclude_flag" = "yes"; then
	AC_DEFINE(HAVE_TAR_EXCLUDE_FLAG,[1],[Define to 1 is tar has --exclude option])
fi

# Check to see if tar supports --files-from
echo "configure" > _test_file
CHECK_TAR_OPTION([--files-from=_test_file],[_cv_gnu_tar_files_from_flag])
if test "$_cv_gnu_tar_files_from_flag" = "yes"; then
	AC_DEFINE(HAVE_TAR_FILES_FROM_FLAG,[1],[Define to 1 is tar has --files-from option])
fi
rm -f _test_file

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
AC_PATH_PROG(ac_gzip,gzip,no,[$PATH])
if test $ac_gzip = no; then
    AC_MSG_ERROR( [gzip is required] )
fi

# true is used in the imake build system, esp. in condor_tests
REQUIRE_PATH_PROG([TRUE],[true])
AC_SUBST(TRUE,$TRUE)

AC_PATH_PROG(ac_purify,purify,no,[$PATH])
if test "$ac_purify" = "purify"; then
   AC_MSG_CHECKING([purify cache directory ($ac_cv_purecachedir)])
   if test -d $ac_cv_purecachedir && test -w $ac_cv_purecachedir ; then
      AC_MSG_RESULT( exists )
   else
      AS_MKDIR_P($ac_cv_purecachedir)
      AC_MSG_RESULT( created )
   fi
fi

# Allow the user to specify a directory where vmware is to be found.
# This logic is a little scary since vm_arg can be "no", "yes", or a path.
AC_ARG_WITH(vmware,
 AC_HELP_STRING([--with-vmware=DIR],
  [full path to directory where the vmware program is located]),
 [vm_arg=$withval],
 [vm_arg=$PATH])

# Depending what the user told us, we will modify vm_path.
if test $vm_arg = "no"; then
 # we don't even check for VMware so assign the makefile var to nothing.
 ac_vmware=""
else
 # Here we check for it with $PATH or with what the user told us where to find
 # it.
 if test $vm_arg = "yes"; then
  vm_path=$PATH
 else
  vm_path=$vm_arg
 fi
 # now find the vmware program, if applicable.
 AC_PATH_PROG(ac_vmware,vmware,no,[$vm_path])
 if test $ac_vmware = "no"; then
  ac_vmware=""
 else
  AC_DEFINE([HAVE_VMWARE],[1],[Define if vmware is available])
 fi
fi
AC_SUBST(VMWARE,$ac_vmware)

AC_PATH_PROG(ac_javac,javac,no,[$PATH])
if test $ac_javac = "no"; then
  AC_MSG_WARN( [javac not found] )
else
  AC_DEFINE([HAVE_JAVAC],[1],[Define if javac is available])
fi
AC_SUBST(JAVAC,$ac_javac)

AC_PATH_PROG(ac_jar,jar,no,[$PATH])
if test $ac_jar = "no"; then
  AC_MSG_WARN( [jar not found] )
else
  AC_DEFINE([HAVE_JAR],[1],[Define if jar is available])
fi
AC_SUBST(JAR,$ac_jar)

# Test for md5sum
AC_PATH_PROG(ac_md5sum,md5sum,no,[$PATH])
if test $ac_md5sum = "no"; then
  AC_MSG_WARN( [md5sum not found] )
else
  AC_DEFINE([HAVE_MD5SUM],[1],[Define if md5sum is available])
fi
AC_SUBST(MD5SUM,$ac_md5sum)

# Test for sha1sum
AC_PATH_PROG(ac_sha1sum,sha1sum,no,[$PATH])
if test $ac_sha1sum = "no"; then
  AC_MSG_WARN( [sha1sum not found] )
else
  AC_DEFINE([HAVE_SHA1SUM],[1],[Define if sha1sum is available])
fi
AC_SUBST(SHA1SUM,$ac_sha1sum)

# By default, we check to see if we can produce md5 integrity checks on the
# released tarballs, rpms, and whatever else.
if test "x$enable_checksum_md5" = "xyes"; then
   # If this is defined to 1, then the checksums are *required* and produced.
   # If it is undefined, then the checksum may or may not be produced
   # depending upon what was available on the machine.
   AC_DEFINE([ENABLE_CHECKSUM_MD5], [1], [Define if md5 checksums are required for released packages])
fi

# By default, we check to see if we can produce sha1 integrity checks on the
# released tarballs, rpms, and whatever else.
if test "x$enable_checksum_sha1" = "xyes"; then
   # If this is defined to on, then the checksums are *required* and produced.
   # If it is undefined, then the checksum may or may not be produced
   # depending upon what was available on the machine.
   AC_DEFINE([ENABLE_CHECKSUM_SHA1], [1], [Define if sha1 checksums are required for released packages])
fi

AC_ARG_ENABLE([rpm],
  AC_HELP_STRING([--enable-rpm],
    [determine if we try to make rpms (default: yes)]),
  [],
  [enable_rpm=yes])
# there might be two kinds of rpm on a machine, 'rpm' and 'rpmbuild'
# use the latter if we have it.
if test "x$_cv_linux_distro" = "xSLES9"; then
	# rpm is broken on some platforms, so just turn them off.
  	AC_MSG_WARN( [rpm generation has been disabled for this architecture] )
	_cv_have_rpm="NO"
else
	# everyone else gets a normal check.
	AC_PATH_PROG(ac_rpmcmd,rpmbuild,no,[$PATH])
	if test $ac_rpmcmd = "no"; then
		AC_PATH_PROG(ac_rpmcmd,rpm,no,[$PATH])
		if test $ac_rpmcmd = "no"; then
  			AC_MSG_WARN( [rpm/rpmbuild not found, so no rpms on this architecture] )
  			_cv_have_rpm="NO"

			# This is ok since MakeRPM is blank in Project.tmpl
			AC_SUBST(RpmCmd,[])
		else
  			_cv_have_rpm="YES"
			AC_SUBST(RpmCmd,$ac_rpmcmd)
		fi
	else
  	_cv_have_rpm="YES"
	AC_SUBST(RpmCmd,$ac_rpmcmd)
	fi
fi
AS_IF([test "x$enable_rpm" != xno -a "x$_cv_have_rpm" != xNO],
  [AC_DEFINE([HAVE_RPM],[1],[Define if making rpms])])

AC_ARG_ENABLE([malloc-debug],
  AC_HELP_STRING([--enable-malloc-debug],
    [enable memory allocation debugging (default: no)]),
  [],
  [enable_malloc_debug=no])

AS_IF([test "x$enable_malloc_debug" = xyes],
  [CPPFLAGS="$CPPFLAGS -DMALLOC_DEBUG"])

AC_ARG_ENABLE([nest],
  AC_HELP_STRING([--enable-nest],
    [enable NeST functionality (default: no)]),
  [],
  [enable_nest=no])
AS_IF([test "x$enable_nest" = xyes],
  [AC_DEFINE([WANT_NEST], [1], [Define if enabling NeST])])

AC_ARG_ENABLE([quill],
  AC_HELP_STRING([--enable-quill],
    [enable Quill functionality (default: yes)]),
  [],
  [enable_quill=yes])
AS_IF([test "x$enable_quill" = xyes],
  [AC_DEFINE([WANT_QUILL], [1], [Define if enabling Quill])])

AC_PATH_XTRA
X_LDFLAGS="$X_LIBS"
X11_LIBS="$X_PRE_LIBS $X_EXTRA_LIBS -lX11"

AC_ARG_ENABLE([kbdd],
  AC_HELP_STRING([--enable-kbdd],
    [enable KBDD functionality (default: platform dependent)]),
  [],
  [case "$_cv_opsys" in
    AIX | HPUX | SOLARIS )
      enable_kbdd=no
    ;;
    LINUX )
      enable_kbdd=yes
    ;;
    * ) # Who had this turned on? No .cf files on 1/6/8 used it. -matt
      enable_kbdd=no
    ;;
  esac])
AS_IF([test "x$enable_kbdd" = xyes],
  [AC_DEFINE([NEEDS_KBDD], [1], [Define if enabling KBDD])
   AC_SUBST(X11_CFLAGS, $X_CFLAGS)
   AC_SUBST(X11_LDFLAGS, $X_LDFLAGS)
   AC_SUBST(X11_LIBS, $X11_LIBS)],
  [AC_SUBST(X11_CFLAGS,"")
   AC_SUBST(X11_LIBS,"")
   AC_SUBST(X11_LDFLAGS, "")])

AC_ARG_ENABLE([hdfs],
  AC_HELP_STRING([--enable-hdfs],
    [enable hadoop filesystem functionality (default: yes)]),
  [],
  [enable_hdfs=yes])
AS_IF([test "x$enable_hdfs" = xyes],
  [AC_DEFINE([WANT_HDFS], [1], [Define if enabling HDFS])])

# Here we figure out whether or not the circumstances are correct for us
# to be able to separate the debugging symbols from the executables and
# package them into a tarball. for example, if say objcopy's debuglink flag
# is available, but we can't exclude the files when making the tarballs,
# then turn the whole thing off.
AC_MSG_CHECKING([for ability to produce debuglink tarball])
if test "x$ac_find" != "xno" -a "x$ac_objcopy" != "xno" \
		-a "x$_cv_objcopy_keepdebug_flag" != "x" \
		-a "x$_cv_gnu_tar_exclude_flag" != "x" \
		-a "x$_cv_gnu_tar_files_from_flag" != "x" ; then
	AC_MSG_RESULT([yes])
	AC_DEFINE(HAVE_DEBUGLINK_TARBALL,[1],[Define to 1 of you know you can produce the debuglink tarball])
else
	AC_MSG_RESULT([no])
fi

############################################################
# checks for libraries
############################################################

# Currently in Condor, threading in daemon core on Unix requires
# both pthreads.
# HACK: for now the code uses Linux-specific code to implement
# recursive mutexes, so instead of using threads wheverever we have
# pthreads, we limit testing for pthreads to just Linux.
if test $os = "Linux"; then
	AC_CHECK_LIB([pthread], [pthread_create],
  		[ac_pthreads=yes
   		AC_DEFINE([HAVE_PTHREADS],[1],[Define if pthreads are available])
		AC_DEFINE([HAS_PTHREADS],[1],[Define if pthreads are available for DRMAA])
		AC_CHECK_LIB([pthread],[pthread_sigmask],AC_DEFINE([HAVE_PTHREAD_SIGMASK],[1],[Define to 1 of you have the pthread_sigmask function]))
		],
  		[ac_pthreads=no])
fi

############################################################
# checks for header files
############################################################

AC_CHECK_HEADERS([net/if.h],[],[],[
#include<netinet/in.h>])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([sys/param.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([os_types.h])
AC_CHECK_HEADERS([sys/personality.h])
AC_CHECK_HEADERS([linux/personality.h])
AC_CHECK_HEADERS([linux/magic.h])
AC_CHECK_HEADERS([linux/nfsd/const.h])
AC_CHECK_HEADERS([linux/sockios.h])
AC_CHECK_HEADERS([linux/types.h])
AC_CHECK_HEADERS([linux/ethtool.h],[],[],[
#if HAVE_OS_TYPES_H
# include <os_types.h>
#endif
#if HAVE_LINUX_TYPES_H
# include <linux/types.h>
#endif])
AC_CHECK_HEADERS([sys/statfs.h])
AC_CHECK_HEADERS([sys/statvfs.h])
AC_CHECK_HEADERS([sys/vfs.h])
AC_CHECK_HEADERS([sys/mount.h],[],[],[
#if HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif
#if HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif])
AC_CHECK_HEADERS([ustat.h])
AC_CHECK_HEADERS([dlfcn.h])
AC_CHECK_HEADERS([resolv.h],[],[],[
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/nameser.h>])
AC_CHECK_HEADERS([pthread.h])
AC_CHECK_HEADERS([ldap.h])

AC_CHECK_LIB([sasl], [sasl_client_init],
  AC_DEFINE([HAVE_LIBSASL], [1], [check for usable libsasl]))
AC_CHECK_LIB([sasl2], [sasl_client_init],
  AC_DEFINE([HAVE_LIBSASL2], [1], [check for usable libsasl2]))


############################################################
# checks for types
############################################################

AC_CHECK_TYPES([int64_t])
AC_CHECK_TYPES([long long])
AC_CHECK_TYPES([__int64])
AC_CHECK_TYPES([id_t])

# checks for structures

# checks for compiler characteristics

############################################################
# checks for library functions
############################################################

AC_CHECK_FUNCS([strsignal				\
		strcasestr				\
		lstat					\
		lstat					\
		lstat64					\
		fstat64					\
		_stati64				\
		_lstati64				\
		_fstati64				\
		stat64					\
		lstat64					\
		fstat64					\
		_stati64				\
		_lstati64				\
		_fstati64				\
		fseeko					\
		ftello					\
		lchown					\
		dirfd					\
		getdirentries				\
		getdtablesize				\
		getpagesize				\
		getwd					\
		setegid					\
		seteuid					\
		setlinebuf				\
		snprintf				\
		setenv					\
		unsetenv				\
		execl					\
		access					\
		vasprintf				\
		clearenv])
AC_CHECK_FUNC(crypt, ,AC_CHECK_LIB(crypt,crypt))
AC_SEARCH_LIBS([dlopen], [dl],
  AC_DEFINE([HAVE_DLOPEN], [1], [dlopen function is available]))

AC_CHECK_FUNCS(backtrace)
#########
# determine if OS has an sched_setaffinity.  Note that Linux has
# three different interfaces to this function over the years.  We
# just check for the most recent.
#########
AC_MSG_CHECKING([if os supports newest sched_setaffinity])

echo "#include <sched.h>" > affinity_test.cpp
echo "void f() {cpu_set_t s;sched_setaffinity(0,1024, &s);}" >> affinity_test.cpp
_test_output=`gcc -c affinity_test.cpp 2>&1`
_test_status=$?
if test $_test_status -eq 0 
then
  AC_MSG_RESULT([yes])
  AC_DEFINE([HAVE_SCHED_SETAFFINITY], [1], [does os support the sched_setaffinity])
else
  AC_MSG_RESULT([no])
fi
rm -f affinity_test.cpp affinity_test.o

# If we can't find clone() on linux (for fast forking from large
# processes), then somebody better understand why; so produce a fatal
# error here.  Under IA64, there is a different interface to clone
# (clone2), and the header files appear to be out of sync with the
# library, so avoid using clone() on that architecture.
if test $os = "Linux" && test $arch != "ia64"; then
  AC_CHECK_DECLS([clone,CLONE_VM,CLONE_VFORK],[AC_DEFINE([HAVE_CLONE],[1],[Define to 1 to use clone() for fast forking])],[AC_MSG_ERROR( [expecting to find clone() on this platform] )],[#include<sched.h>])
fi

AC_CHECK_DECLS([res_init],[],[],[
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/nameser.h>
#include<resolv.h>])

AC_CHECK_FUNCS(statfs statvfs)
dnl
dnl file system type member in statfs struct
dnl
AC_CHECK_MEMBERS([struct statfs.f_type], ,[
  AC_CHECK_MEMBERS([struct statfs.f_fstyp], , , [#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif])], [#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif])

AC_CHECK_MEMBERS([struct statfs.f_fstypename], , , [#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif])

AC_CHECK_MEMBERS([struct statvfs.f_fsid], , , [#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif])

AC_CHECK_MEMBERS([struct statvfs.f_basetype], , , [#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif])

dnl
dnl if statfs() takes 2 arguments or 4 (Solaris)
dnl
if test "$ac_cv_func_statfs" = yes ; then
  AC_MSG_CHECKING([number of arguments to statfs()])
  AC_TRY_COMPILE([#include <unistd.h>
  #ifdef HAVE_SYS_TYPES_H
  #include <sys/types.h>
  #endif
  #ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
  #endif
  #ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
  #endif
  #ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
  #endif
  #ifdef HAVE_SYS_STATFS_H
  #include <sys/statfs.h>
  #endif
  #ifdef HAVE_USTAT_H
  #include <ustat.h>
  #endif], [struct statfs st;
  statfs(NULL, &st);],[
    AC_MSG_RESULT([2])
    AC_DEFINE(STATFS_ARGS, 2, [Number of arguments to statfs()])],[
    AC_TRY_COMPILE([#include <unistd.h>
  #ifdef HAVE_SYS_PARAM_H
  #include <sys/param.h>
  #endif
  #ifdef HAVE_SYS_VFS_H
  #include <sys/vfs.h>
  #endif
  #ifdef HAVE_SYS_MOUNT_H
  #include <sys/mount.h>
  #endif
  #ifdef HAVE_SYS_STATFS_H
  #include <sys/statfs.h>
  #endif], [struct statfs st;
  statfs(NULL, &st, sizeof (st), 0);],[
      AC_MSG_RESULT([4])
      AC_DEFINE(STATFS_ARGS, 4, [Number of arguments to statfs()])],[
      AC_MSG_RESULT(unknown)
      AC_MSG_ERROR([unable to determine number of arguments to statfs()])])])
fi dnl test "$ac_cv_func_statfs" = yes

# Do we have the "ethtool" and similar declarations?
AC_CHECK_DECLS([SIOCETHTOOL,SIOCGIFCONF],,, [[#include <linux/sockios.h>]])

# Does net/if.h define struct ifreq ?
AC_CHECK_TYPES([struct ifreq,struct ifconf],[],[],[#include <net/if.h>])

# we don't actually want to add -lgen to our link line, but we do want
# HAVE_LIBGEN defined correctly for our test suite builds...
AC_CHECK_FUNC(dirname,,AC_CHECK_LIB(gen,dirname,
AC_DEFINE(HAVE_LIBGEN,1,[Define to 1 if you have the `gen' library (-lgen)])))

AC_CHECK_FUNCS(mkstemp tmpnam)
##############################################################################
AC_CHECK_LIB(c,crypt,ac_has_crypt=1,ac_has_crypt=0)

if test "$ac_has_crypt" = "1" ; then
    _cv_crypt_lib=""
    AC_MSG_RESULT([-lc])
else
    AC_CHECK_LIB(crypt,crypt,ac_has_crypt=1,ac_has_crypt=0)

    if test "$ac_has_crypt" = "1" ; then
        _cv_crypt_lib="-lcrypt"
    else
        AC_MSG_ERROR([You need to find a crypt definition])
    fi
fi
AC_SUBST([crypt_lib],$_cv_crypt_lib)

# Check whether we have a well-behaved snprintf()
AC_FUNC_SNPRINTF

##############################################################################


# checks for system services

# checks for platform-specific settings we don't yet auto-detect

AC_ARG_ENABLE([static],
  AC_HELP_STRING([--enable-static],
    [determine if we do static linking (default: yes)]),
  [],
  [enable_static=yes])

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
  "Linux" )
    _cv_has_static=yes
    ;;
  "SunOS" )
    _cv_has_static=no
    ;;
  "Darwin" )
    _cv_has_static=no
    ;;
  "FreeBSD" )
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
AC_MSG_CHECKING([if we are going to statically link])
AS_IF([test "x$enable_static" != xno -a "$_cv_has_static" = "yes"],
  [AC_MSG_RESULT([yes])
   AC_SUBST(_cv_has_static,YES)],
  [AC_MSG_RESULT([no])
   AC_SUBST(_cv_has_static,NO)])


############################################################
# What versions of what externals do we need
############################################################

#
# Check for externals, if we are going to use them
#

if test "x$ac_cv_has_externals" = "xYES" -o "x$enable_proper" = xyes ; then

AC_MSG_NOTICE([determining availability of external packages])

#
# Define a number of _cv_has_<external>s for externals that are not
# available on all platforms
#

#-----------------------------------------------------------------
# NOTE: the checks for globus and SSL here must happen *outside*
# the check for $ac_cv_has_externals, since we want these to be
# set whether or not we're actually trying to build the externals
# or not.  for example, the test suite needs to know these things,
# even though it's being configured using --without-externals.
#-----------------------------------------------------------------
# determine whether we will be using openssl and globus on this platform.
# This logic used to be in the individual <platform>.cf files;
AC_MSG_CHECKING([if openssl is supported])
case "$os" in
  HP-UX ) _cv_has_openssl=no ;;
  Linux|SunOS|Darwin|FreeBSD|AIX ) _cv_has_openssl=yes ;;
  * )
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([Unrecognized platform: $os])
  ;;
esac
AC_MSG_RESULT([$_cv_has_openssl])

AC_MSG_CHECKING([if gsoap is supported])
case "$os" in
  HP-UX )
	if  test "x$_cv_sysname" = "xhp_ux113" ; then
  	   _cv_has_gsoap=no
	else
  	   _cv_has_gsoap=yes
	fi
	;;
  Linux|SunOS|Darwin|FreeBSD|AIX ) _cv_has_gsoap=yes ;;
  * )
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([Unrecognized platform: $os])
  ;;
esac
AC_MSG_RESULT([$_cv_has_gsoap])

AC_MSG_CHECKING([if krb5 is supported])
case "$os-$_cv_arch" in
  HP-UX-*|Linux-ALPHA|Linux-CONDOR_PPC ) _cv_has_krb5=no ;;
  Linux-*|SunOS-*|Darwin-*|FreeBSD-*|AIX-* ) _cv_has_krb5=yes ;;
  * )
    AC_MSG_RESULT([error])
    AC_MSG_ERROR([Unrecognized platform: $os-$_cv_arch])
  ;;
esac
AC_MSG_RESULT([$_cv_has_krb5])

AC_MSG_CHECKING([if classads is supported])
# currently, all platforms support the classad external
_cv_has_classads=yes
AC_MSG_RESULT([$_cv_has_classads])


#
# Figure out if I need to use the linuclibcheaders package to build glibc.
#

# assume no, unless I discover otherwise.
llh_use=NO

# if this is a linux machine, determine if I need the linux libc headers to
# build glibc
if test "x$os" = "xLinux" ; then

	llh_use=YES

	case "x$_cv_kernel_rev" in
		"x2.6.3" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.3.0], [soft])
			;;
		"x2.6.4" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.4.0], [soft])
			;;
		"x2.6.5" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.5.2], [soft])
			;;
		"x2.6.6" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.6.0], [soft])
			;;
		"x2.6.7" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.7.0], [soft])
			;;
		"x2.6.8" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.8.1], [soft])
			;;
		"x2.6.9" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.9.1], [soft])
			;;
		"x2.6.10" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.10.0], [soft])
			;;
		"x2.6.11" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.11.2], [soft])
			;;
		"x2.6.12" )
			CHECK_EXTERNAL([linuxlibcheaders],[2.6.12.0], [soft])
			;;
		*)
			llh_use=NO
			;;
	esac
fi

# Tell the build system I want to use this module since it very well might
# not always be defined or desired to be defined.
if test "x$llh_use" = "xYES" ; then
	AC_SUBST([want_ext_linuxlibcheaders],[YES])
else
	AC_SUBST([want_ext_linuxlibcheaders],[NO])
  	AC_SUBST([ext_linuxlibcheaders_version],[UNUSED])
fi


# For now, we hardcode that non-i386/X86_64 platforms are clipped and don't
# need an external glibc for anything...
if test "x$_cv_libc_kind" = "xGLIBC" -a \( "x$_cv_arch" = "xI386" -o "x$_cv_arch" = "xX86_64" \) ; then

  including_glibc_ext=YES
  _cv_libc_full_vers="$_cv_libc_major.$_cv_libc_minor.$_cv_libc_patch"
  case "$_cv_libc_full_vers" in
  "2.3.2" )
    # the glibc 2.3.2 from redhat's source rpms are very different than the
	# stock 2.3.2 you can download off of the web.
	# This external is used both for x86 and x86_64
    CHECK_EXTERNAL([glibc],[2.3.2.27.9.7], [soft])
    ;;
  "2.3.4" )
	# I'm using this here to avoid a full port of Condor to the centos 4 OS.
	# XXX Also, one must use gcc 3.2.2 instead of as shipped gcc 3.4.4 to
	# compile this package.
    CHECK_EXTERNAL([glibc],[2.3.2.27.9.7], [soft])
    ;;
  "2.3.6" )
    CHECK_EXTERNAL([glibc],[2.3.2.27.9.7], [soft])
    ;;
  "2.5." ) # RHEL 5.0/5.1
    if test "x$_cv_arch" = "xI386" ; then
       CHECK_EXTERNAL([glibc],[2.5-20061008T1257-p0], [soft])
	else # x86_64
       CHECK_EXTERNAL([glibc],[2.5-20061008T1257-x86_64-p0], [soft])
	fi
    ;;
  "2.6." ) # F7 uses glibc 2.6-4
    including_glibc_ext=NO
    ;;
  "2.7." )
 	 # F8 uses glibc 2.7-2
	 # debian uses glibc 2.7-18
    if test "x$_cv_arch" = "xI386" ; then
      CHECK_EXTERNAL([glibc],[2.7-18], [soft])
    else # x86_64
      CHECK_EXTERNAL([glibc],[2.7-18-x86_64], [soft])
    fi
    ;;
  * )
    if test "x$enable_glibc_version_check" = "xyes"; then
      AC_MSG_CHECKING(glibc)
      AC_MSG_RESULT([ERROR])
      AC_MSG_ERROR([Condor does NOT know what glibc external to use with glibc-$_cv_libc_full_vers])
    else
      AC_MSG_CHECKING(glibc)
      AC_MSG_RESULT([glibc-$_cv_libc_full_vers (UNSUPPORTED)])
      including_glibc_ext=NO
    fi
  esac
  if test "x$including_glibc_ext" = "xYES"; then
    AC_SUBST(want_ext_glibc,YES)
  else
    AC_SUBST(want_ext_glibc,NO)
    AC_SUBST(ext_glibc_version,UNUSED)
  fi
else
  AC_SUBST(want_ext_glibc,NO)
  AC_SUBST(ext_glibc_version,UNUSED)
fi

# Google's coredumper is Linux specific and only officially supports
# a few architectures.
if test "$os" = "Linux" -a \( "x$_cv_arch" = "xI386" -o "x$_cv_arch" = "xX86_64" \); then
    CHECK_EXTERNAL([coredumper],[0.2], [soft])
	AC_SUBST(want_coredumper,YES)
else
	AC_MSG_CHECKING(coredumper)
	AC_MSG_RESULT([no])
	AC_SUBST(want_coredumper,NO)
fi

# Build drmaa on any platform with pthreads available, except for the following
# platforms where the build is known to fail
if test "x$_cv_sysname" != "xppc_aix52" -a "x$_cv_linux_distro" != "xRH72" -a  "x$_cv_sysname" != "xsun4x_58" -a "x$_cv_sysname" != "xsun4x_59" -a "x$_cv_sysname" != "xalpha_dux51" -a "x$_cv_sysname" != "xhp_ux113" ; then
  if test "x$_cv_sysname" = "xppc_macosx" -o "x$ac_pthreads" = "xyes"; then
   CHECK_EXTERNAL([drmaa],[1.6], [soft])
  fi
fi

CHECK_EXTERNAL([krb5], [1.4.3-p0], [soft],
               [use krb5 (provides Kerberos support)],
               MF_LIB_CHECK([krb5], [[krb5 krb5_free_principal]]))

# CHECK_EXTERNAL([openssl],[0.9.8h-p1],[soft])
CHECK_EXTERNAL([openssl], [0.9.8h-p1], [soft],
               [use OpenSSL (provides authentication and encryption support)],
               MF_LIB_CHECK([openssl],
                            [[ssl SSL_connect], [crypto BF_encrypt]]))

if test "x$_cv_ext_openssl_version" != x; then
  # If we have openssl, we define HAVE_OPENSSL_SSL_H (meaning we
  # have ssl.h) to make gsoap happy
  AC_DEFINE(HAVE_OPENSSL_SSL_H,1,[do we have openssl])

  # Globus 4.2.1 is dependent on an external version of openssl
  CHECK_EXTERNAL([globus],[4.2.1-p3], [soft])
fi

if test -z "$_cv_ext_globus_version"; then
  _cv_has_globus=no
else
  _cv_has_globus=yes
fi

_cv_has_unicoregahp=$_cv_has_globus
CHECK_EXTERNAL([unicoregahp],[1.2.0], [soft])

if test "x$CLIPPED" = xyes; then
  _cv_zlib_requirement=soft
else
  _cv_zlib_requirement=hard
fi
CHECK_EXTERNAL([zlib], [1.2.3], [$_cv_zlib_requirement],
               [use zlib (provides compression support)],
               MF_LIB_CHECK([zlib], [[z deflate]]))

CHECK_EXTERNAL([classads], [1.0.2], [soft],
               [use new ClassAds (provides -better-analyze and more)],
	       # This REALLY should test for headers, like most checks...
               MF_LIB_CHECK([classads], [[classad cclassad_create]]))	       

CHECK_EXTERNAL([srb], [3.2.1-p2], [soft])

AC_MSG_CHECKING([if VOMS should be compiled (linux only)])
if test "$_cv_opsys" = "LINUX" ; then
	_cv_including_voms=yes
	AC_MSG_RESULT([$_cv_including_voms])
	CHECK_EXTERNAL([expat], [2.0.1], [soft])
	CHECK_EXTERNAL([voms], [1.8.9-p1], [soft])
else
	_cv_including_voms=no
	AC_MSG_RESULT([$_cv_including_voms])
fi

if test "x$_cv_linux_distro" = "xRHEL5" -o "x$_cv_linux_distro" = "xRHEL3" -a "$_cv_arch" = "xI386" ; then
  CHECK_EXTERNAL([cream],[1.10.1], [soft])
fi

CHECK_EXTERNAL([pcre], [7.6], [hard],
               [use PCRE (provides regular expression support)],
               MF_LIB_CHECK([pcre], [[pcre pcre_compile]], [PCRE]))
AS_IF([test "x$cv_ext_pcre" = xyes],
  [CPPFLAGS_save="$CPPFLAGS"
   CPPFLAGS="$CPPFLAGS $PCRE_CFLAGS"
   AC_CHECK_HEADERS([pcre.h pcre/pcre.h])
   CPPFLAGS="$CPPFLAGS_save"])
AC_SUBST(PCRE_LDFLAGS, $PCRE_LDFLAGS)
AC_SUBST(PCRE_CFLAGS, $PCRE_CFLAGS)

# The auto-tools aren't working correctly on AIX
# Solaris doesn't have the auto-tools on NMI
# FreeBSD has different types for network structs
# Mac OS X doesn't have strndup()
AS_IF([test "x$_cv_opsys" != xDARWIN -a "x$_cv_opsys" != xAIX -a "x$_cv_opsys" != "xSOLARIS" -a "x$_cv_opsys" != "xCONDOR_FREEBSD" -a "x$_cv_has_globus" = xyes -a -n "$_cv_ext_classads_version"],
  [_cv_has_blahp=yes],
  [_cv_has_blahp=no])
AC_MSG_RESULT([$_cv_has_blahp])
CHECK_EXTERNAL([blahp], [1.12.2-p5], [soft])

# When --with-proper the detection of the man pages is a little
# ugly. The value added to the end of $_dir must be what
# man-current.tar.gz extracts to, ugh. The manual is a very unique
# external. -matt 083107
CHECK_EXTERNAL([man], [current], [soft],
               [include the man pages (required for a release)],
               [AS_IF([test "x$_dir" = x],
                 [_dir="../externals/bundles/man/current"])
                AS_IF([test -f "$_dir/man-current.tar.gz"],
                 [cv_ext_man=yes],
                 [AC_MSG_WARN([unable to find $_dir/man-current.tar.gz])
                  cv_ext_man=no])
                AC_SUBST([man_dir], [$_dir/man])])


# temporary hack until we can reliably build BOINC everywhere.  for
# now, just enable it manually, and pretend we've got it.  since we're
# not actually linking anything with BOINC in phase 1, we don't *need*
# the external to enable the BOINC + Backfill code paths.
AC_DEFINE(HAVE_BACKFILL,1,[are we compiling support for any backfill systems])
AC_DEFINE(HAVE_BOINC,1,[are we compiling support for backfill with BOINC])

# GCB only matters and works if we have GNU ld...
if test x$_cv_ld_is_gnu = xyes; then
  CHECK_EXTERNAL([gcb], [1.5.6], [soft])
fi

AX_LIB_ORACLE_OCI("10.0.0")
if test $HAVE_ORACLE_OCI = yes; then
  AC_DEFINE(HAVE_ORACLE,1,[Do we have Oracle support])
fi

# Figure out which postgresql we need.

if test "$_cv_opsys" = "AIX" ; then
  CHECK_EXTERNAL([postgresql], [8.2.3], [soft],
                 [use PostgreSQL (provides Quill and other support)],
                 MF_LIB_CHECK([postgresql], [[pq PQconnectdb]], [postgresql]))
else
  CHECK_EXTERNAL([postgresql], [8.2.3-p1], [soft],
                 [use PostgreSQL (provides Quill and other support)],
                 MF_LIB_CHECK([postgresql], [[pq PQconnectdb]], [postgresql]))
fi
AC_MSG_CHECKING([postgresql])
AC_MSG_RESULT([$_cv_ext_postgresql_version])
#AC_SUBST(ext_postgresql_version,$_cv_ext_postgresql_version)
AC_SUBST(POSTGRESQL_LDFLAGS, $POSTGRESQL_LDFLAGS)
AC_SUBST(POSTGRESQL_CFLAGS, $POSTGRESQL_CFLAGS)

# if globus is available on this platform and we actually have it
if test "x$cv_ext_globus" = xyes; then
  AC_MSG_CHECKING([globus flavor])
  _cv_globus_flavor=gcc32dbg
  if test "$_cv_arch" = "ALPHA" -o "$_cv_arch" = "IA64" -o "$_cv_arch" = "X86_64"; then
    _cv_globus_flavor=gcc64dbg
  elif test "$_cv_opsys" = "AIX" ; then
    _cv_globus_flavor=vendorcc32dbg
  fi
  _cv_globus_thr_flavor=${_cv_globus_flavor}pthr
  AC_MSG_RESULT([$_cv_globus_flavor])
  AC_SUBST(globus_flavor,$_cv_globus_flavor)
  AC_SUBST(globus_thr_flavor,$_cv_globus_thr_flavor)
fi

AC_ARG_ENABLE(ssh-to-job,
  [AS_HELP_STRING([--enable-ssh-to-job],
     [Support for condor_ssh_to_job])],
  [SSH_TO_JOB=$enableval],
  [if test "x$cv_ext_openssl" = xyes; then SSH_TO_JOB=yes; else SSH_TO_JOB=no; fi])
if test $SSH_TO_JOB != no; then
  if test "x$cv_ext_openssl" != xyes; then
    AC_MSG_ERROR( [condor_ssh_to_job requires openssl (for base64)] )
  fi
  AC_DEFINE([HAVE_SSH_TO_JOB], [1], [Define to 1 to support condor_ssh_to_job])
fi

# Look for the curl external library, used by the file transfer object
CHECK_EXTERNAL([curl],[7.19.6-p1],[soft])

#Look for hadoop jars, required by HDFS daemon as well by hdfs file plugin
# Only build hadoop external if hdfs is enabled
AS_IF([test "x$enable_hdfs" = xyes], 
  [CHECK_EXTERNAL([hadoop],[0.20.0-p2],[soft])])

if test "x$_cv_opsys" = "xLINUX" ; then
  CHECK_EXTERNAL([libxml2],[2.7.3],[soft],
                 [libxml2 for libvirt],
                 [MF_LIB_CHECK([libxml2],
                               [[xml2 xmlInitGlobals]], [libxml2])])
fi

if test "x$_cv_opsys" = "xLINUX" ; then
  AC_CHECK_HEADER([libvirt/libvirt.h], [have_libvirt_h=yes])
  CHECK_EXTERNAL([libvirt],[0.6.2],[soft],
                 [libvirt for Xen/KVM/etc support on Linux],
                 [AS_IF([test "x$have_libvirt_h" =  xyes],
                   [MF_LIB_CHECK([libvirt], [[virt virGetVersion]], [libvirt])],
                   [cv_ext_libvirt=no])])
fi

# The test for gsoap needs to happen after the openssl check, so
# openssl libs, if found, will be in LIBS. Also, if we do not have
# openssl, then we need to link with
# a non ssl version of the gSOAP libraries.

# The gsoap shared libraries contain unresolved symbols they expect
# the program they are linking with to implement. To avoid errors
# because of this we need to provide some extra linker flags.

# NOTE: We don't need the --unresolved-symbols flag in the non-proper
# case because in that case Condor links with the static libraries,
# and the linker does not complain as much.
CHECK_EXTERNAL([gsoap], [2.7.10-p3], [soft],
               [use gSOAP (enables Birdbath interface)],
               [_ldflags_save="$LDFLAGS"
                LDFLAGS="$LDFLAGS -Wl[,]--unresolved-symbols=ignore-in-shared-libs"
                AS_IF([test "x$cv_ext_globus" = xyes -o "x$cv_ext_openssl" = xyes],
                  [MF_LIB_CHECK([gsoap],
                                [[gsoapssl++ soap_ssl_accept]], [gsoap])],
                  [MF_LIB_CHECK([gsoap],
                                [[gsoap++ soap_accept]], [gsoap])])
                AS_IF([test "x$cv_ext_gsoap" = xno],
                  [LDFLAGS="$_ldflags_save"])])
AC_SUBST(GSOAP_LDFLAGS, $GSOAP_LDFLAGS)
AC_SUBST(GSOAP_CFLAGS, $GSOAP_CFLAGS)

# We need to make sure HAVE_OPENSSL_SSL_H is defined otherwise
# programs will not link properly as stdsoap2.h will not include ssl
# structures. This is set manually if using Globus's OpenSSL.
AS_IF([test "x$cv_ext_gsoap" = xyes -a "x$enable_proper" = xyes],
  [AC_CHECK_HEADERS([openssl/ssl.h], [],
    [AC_MSG_ERROR([gsoap in proper mode requires openssl/ssl.h])])])

# Before the --unresolved-symbols flag was founded we needed to use
# the following test. We will keep it around incase it is needed
# again, for linkers that do not support --unresolved-symbols, or
# --allow-shlibs-undefined. Note, this test does not let all Condor
# programs compile that have gsoap on the link line, because of the
# undefined symbols.
#
## Also, gSOAP is special in that its libraries are really frameworks,
## so we need to define symbols the library expects or configure cannot
## tell the difference between a missing symbol we are testing for or a
## missing expected symbol. We cannot define these symbols with
## ac_check_lib so we have to use ac_link_ifelse.
##               [
##  _libs_save=$LIBS
##  LIBS="$LIBS -lgsoapssl++"
##  AC_LINK_IFELSE([
##    #ifdef __cplusplus
##    extern "C" {
##    #endif
##    int soap_putheader;
##    int soap_serializeheader;
##    int soap_faultstring;
##    int soap_faultsubcode;
##    int soap_faultcode;
##    int soap_getheader;
##    int soap_getfault;
##    int soap_faultdetail;
##    int soap_putfault;
##    int soap_serializefault;
##    char soap_ssl_accept();
##    int main() {
##      return soap_ssl_accept();
##    }
##    #ifdef __cplusplus
##    }
##    #endif
##    ],
##   [cv_ext_gsoap=yes],
##   [cv_ext_gsoap=no])
##  LIBS=$_libs_save])


# This MUST happen after externals are determined because it must
# reference information about externals, specifically glibc
#
# SIMPLE_LIBC is needed for unclipped builds, as is LIBC
#
# It turns out that SIMPLE_LIBC==LIBC
#

AC_MSG_CHECKING([if we have libc.a])
_cv_libc_a=none
# from externals.cf.in
GLIBC_DIR="$ac_cv_externals_build/install/$_cv_ext_glibc_version/lib"
case "$_cv_opsys" in
# Ignore on clipped ports
#  ?ALPHA_OSF1? ) /lib/libc.a ;;
#  ?HPUX? ) /lib/libc.a ;;
#  ?HPUX11? ) /usr/lib/pa20_64/libc.a ;;
  LINUX )
    if test "x$CLIPPED" = xyes; then
      _cv_libc_a=/usr/lib/libc.a
    else
      _cv_libc_a=$GLIBC_DIR/libc.a
    fi
  ;;
  SOLARIS ) _cv_libc_a=/lib/libc.a ;;
esac
AC_MSG_RESULT([$_cv_libc_a])

AC_SUBST([LIBC_A], [$_cv_libc_a])

else
# if there's no externals at all, we still need to define these to
# avoid imake troubles...
  AC_SUBST([want_ext_linuxlibcheaders],[NO])
  AC_SUBST([ext_linuxlibcheaders_version],[UNUSED])
  AC_SUBST(want_ext_glibc,NO)
  AC_SUBST(ext_glibc_version,UNUSED)

fi
# END of if $ac_cv_has_externals = "YES" or $enable_proper = yes

# This, unfortunately, must be way down here to make sure info about
# externals is defined. -matt 1/6/8
AC_ARG_ENABLE([stork],
  AC_HELP_STRING([--enable-stork],
    [enable Stork functionality (default: platform dependent)]),
  [],
  [if test "x$_cv_opsys" = xLINUX -a "x$_cv_arch" = xI386 \
       -a "x$_cv_has_globus" = xyes -a "x$cv_ext_globus" = xyes \
       -a "x$_cv_has_classads" = xyes -a "x$cv_ext_classads" = xyes \
       -a "x$_cv_libc_kind-$_cv_libc_major-$_cv_libc_minor" = xGLIBC-2-3; then
    enable_stork=yes;
   else
    enable_stork=no;
   fi])
AS_IF([test "x$enable_stork" = xyes],
  [AC_DEFINE([WANT_STORK], [1], [Define if enabling Stork])])

# NRL: Similar logic for the lease manager
AC_ARG_ENABLE([lease-manager],
  AC_HELP_STRING([--enable-lease-manager],
    [enable lease manager functionality (default: platform dependent)]),
  [],
  [if test "$_cv_has_classads" = "yes" -a "$cv_ext_classads" = "yes" ; then
    enable_lease_manager=yes;
   else
    enable_lease_manager=no;
   fi])
AS_IF([test "$enable_lease_manager" = "yes"],
  [AC_DEFINE([WANT_LEASE_MANAGER], [1],[Define if enabling lease manager])])


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
