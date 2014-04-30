# Macros defined in .m4 files that are no longer present.
# These are in aclocal.m4, but that file gets overwritten when running
#  the bootstrap.sh script.

dnl Usage:
dnl AC_BOOST(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - BOOST_CFLAGS (compiler flags)
dnl - BOOST_LIBS (linker flags, stripping and path)
dnl - BOOST_FS_LIBS (linker flags, stripping and path)
dnl - BOOST_THREAD_LIBS (linker flags, stripping and path)
dnl - BOOST_REGEX_LIBS (linker flags, stripping and path)
dnl - BOOST_PYTHON_LIBS (linker flags, stripping and path)
dnl - BOOST_PO_LIBS (linker flags, stripping and path)
dnl - BOOST_INSTALL_PATH


AC_DEFUN([AC_BOOST],
[
    have_boost_default=no
    AC_BOOST_DEFAULT([],have_boost_default=yes,have_boost_default=no)
    
    if test "x$have_boost_default" = "xyes" ; then
        ifelse([$2], , :, [$2])
    else
        have_boost_old=no
        AC_BOOST_OLD([],have_boost_old=yes,have_boost_old=no)
        
        if test "x$have_boost_old" = "xyes" ; then
            ifelse([$2], , :, [$2])
        else
            ifelse([$3], , :, [$3])
        fi
    fi

])

AC_DEFUN([AC_BOOST_DEFAULT],
[
  AC_ARG_WITH(
    boost-prefix, 
    AC_HELP_STRING(
      [--with-boost-prefix=DIR],
      [root of the boost installation]
    ),
    [ac_boost_prefix=$withval],
    [ac_boost_prefix="/usr"]
  )

  AC_ARG_ENABLE(
    boost-mt,
    AC_HELP_STRING(
      [--enable-boost-mt],
      [use multithreaded boost libraries (default = yes)]
    ),
    [ac_boost_mt=$enableval],
    [ac_boost_mt="yes"]
  )

  AC_MSG_RESULT([boost install dir: $ac_boost_prefix])
  AC_MSG_RESULT([using boost mt: $ac_boost_mt])
  
  BOOST_INSTALL_PATH=${ac_boost_prefix}
  BOOST_CFLAGS="-I${ac_boost_prefix}/include"
  
  if test -d "$ac_boost_prefix/lib64" ; then
    ac_boost_libdir=${ac_boost_prefix}/lib64
  else
    ac_boost_libdir=${ac_boost_prefix}/lib
  fi
  
  if test -e ${ac_boost_libdir}/libboost_thread-mt.so ; then
    BOOST_THREAD_LIBS="-lboost_thread-mt -lpthread"
  else
    BOOST_THREAD_LIBS="-lboost_thread -lpthread"
  fi

  if test -e ${ac_boost_libdir}/libboost_filesystem-mt.so ; then
    BOOST_FS_LIBS="-lboost_filesystem-mt"
  else
    BOOST_FS_LIBS="-lboost_filesystem"
  fi
  
  if test -e ${ac_boost_libdir}/libboost_regex-mt.so ; then
    BOOST_REGEX_LIBS="-lboost_regex-mt"
  else
    BOOST_REGEX_LIBS="-lboost_regex"
  fi
  
  if test -e ${ac_boost_libdir}/libboost_python-mt.so ; then
    BOOST_PYTHON_LIBS="-lboost_python-mt"
  else
    BOOST_PYTHON_LIBS="-lboost_python"
  fi

  if test -e ${ac_boost_libdir}/libboost_program_options-mt.so ; then
    BOOST_PO_LIBS="-lboost_program_options-mt"
  else
    BOOST_PO_LIBS="-lboost_program_options"
  fi

  if test -e ${ac_boost_libdir}/libboost_date_time-mt.so ; then
    BOOST_DATE_LIBS="-lboost_date_time-mt"
  else
    BOOST_DATE_LIBS="-lboost_date_time"
  fi
  
  BOOST_LIBS="-L${ac_boost_libdir} $BOOST_FS_LIBS $BOOST_THREAD_LIBS $BOOST_REGEX_LIBS $BOOST_PO_LIBS $BOOST_DATE_LIBS"
  
  AC_MSG_CHECKING([for boost default installation])
  
  AC_LANG_SAVE
  AC_LANG_CPLUSPLUS
  ac_save_cppflags=$CPPFLAGS
  ac_save_libs=$LIBS
  CPPFLAGS="$BOOST_CFLAGS $CPPFLAGS"
  LIBS="$BOOST_LIBS $LIBS"
  AC_TRY_LINK([ #include <boost/thread/mutex.hpp> ],
              [ boost::mutex   mut; ],
              [ ac_have_boost=yes ], [ ac_have_boost=no ])
  AC_MSG_RESULT([$ac_have_boost])
  CPPFLAGS=$ac_save_cppflags
  LIBS=$ac_save_libs
  AC_LANG_RESTORE

  if test x$ac_have_boost = xyes ; then
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    ac_save_cppflags=$CPPFLAGS
    ac_save_libs=$LIBS
    CPPFLAGS="$BOOST_CFLAGS $CPPFLAGS"
    LIBS="$BOOST_LIBS $LIBS"
    AC_MSG_CHECKING([for stringstream into c++ STL])
    AC_TRY_LINK([ #include <sstream> ],
                [ std::stringstream sstr; ],
                [ ac_have_stringstream=yes ], 
                [ ac_have_stringstream=no 
                  BOOST_CFLAGS="$BOOST_CFLAGS -DBOOST_NO_STRINGSTREAM" ])
    if test x$ac_have_stringstream = xyes ; then
        AC_DEFINE([HAVE_STRINGSTREAM], 1, [Define when we are sure to have the right strstream header])
    else 
        AC_DEFINE([BOOST_NO_STRINGSTREAM], 1, [Define when to remove the stringstream use from the boost library])
    fi 
    AC_MSG_RESULT([$ac_have_stringstream])
    CPPFLAGS=$ac_save_cppflags
    LIBS=$ac_save_libs
    AC_LANG_RESTORE

    ifelse([$2], , :, [$2])
    
  else
    BOOST_INSTALL_PATH=""
    BOOST_CFLAGS=""
    BOOST_LIBS=""
    BOOST_DATE_LIBS=""
    BOOST_FS_LIBS=""
    BOOST_THREAD_LIBS=""
    BOOST_REGEX_LIBS=""
    BOOST_PO_LIBS=""
    BOOST_PYTHON_LIBS=""
    ifelse([$3], , :, [$3])
  fi

  AC_SUBST(BOOST_INSTALL_PATH)
  AC_SUBST(BOOST_CFLAGS)
  AC_SUBST(BOOST_LIBS)
  AC_SUBST(BOOST_DATE_LIBS)
  AC_SUBST(BOOST_FS_LIBS)
  AC_SUBST(BOOST_THREAD_LIBS)
  AC_SUBST(BOOST_REGEX_LIBS)
  AC_SUBST(BOOST_PO_LIBS)
  AC_SUBST(BOOST_PYTHON_LIBS)
])


AC_DEFUN([AC_BOOST_OLD],
[
  AC_ARG_WITH(
    boost-prefix, 
    AC_HELP_STRING(
      [--with-boost-prefix=DIR],
      [root of the boost installation]
    ),
    [ac_boost_prefix=$withval],
    [ac_boost_prefix="/usr"]
  )

  AC_ARG_ENABLE(
    boost-debug,
    AC_HELP_STRING(
      [--enable-boost-debug],
      [use debug boost libraries]
    ),
    [ac_boost_debug=$enableval],
    [ac_boost_debug="no"]
  )

  AC_MSG_RESULT([using boost debug: $ac_boost_debug])

  AC_ARG_ENABLE(
    boost-mt,
    AC_HELP_STRING(
      [--enable-boost-mt],
      [use multithreaded boost libraries (default = yes)]
    ),
    [ac_boost_mt=$enableval],
    [ac_boost_mt="yes"]
  )

  AC_MSG_RESULT([using boost mt: $ac_boost_mt])

  ac_boost_version_file="$ac_boost_prefix/include/boost/version.hpp"
  if test -r "$ac_boost_version_file" ; then
    ac_boost_version=`cat $ac_boost_version_file | grep '^# *define  *BOOST_VERSION  *[0-9]\+$' | sed 's,^# *define  *BOOST_VERSION  *\([0-9]\+\)$,\1,'`
  fi
  AC_MSG_RESULT([boost version: $ac_boost_version])
  if test -z "$ac_boost_version" ; then
    AC_MSG_ERROR(unknow version of boost, [1])
  fi

  AC_MSG_CHECKING([for boost installation])

  if test "$ac_boost_version" -lt 103000 -o "$ac_boost_prefix" = "/usr" ; then

    BOOST_CFLAGS=""

    if test "$ac_boost_version" -lt 103000; then
      BOOST_FS_LIBS="-lboost_fs"
    else
      BOOST_FS_LIBS="-lboost_filesystem"
    fi

    BOOST_THREAD_LIBS="-lboost_thread -lpthread"
    BOOST_REGEX_LIBS="-lboost_regex"
    BOOST_PYTHON_LIBS="-lboost_python"
    BOOST_PO_LIBS="-lboost_program_options"
    BOOST_DATE_LIBS="-lboost_date_time"
    BOOST_LIBS="$BOOST_FS_LIBS $BOOST_THREAD_LIBS $BOOST_REGEX_LIBS $BOOST_PO_LIBS $BOOST_DATE_LIBS"

    if test -n "$ac_boost_prefix" -a "$ac_boost_prefix" != "/usr" ; then
        if test -d "$ac_boost_prefix" ; then
            ac_boost_prefix="$ac_boost_prefix"
            AC_MSG_RESULT([prefix: $ac_boost_prefix])
        else
            ac_boost_prefix="/usr"
            AC_MSG_RESULT([prefix: $ac_boost_prefix])
            AC_MSG_WARN([***  Cannot find an installed Boost library, trying with defaults])
        fi
    elif test -z "$ac_boost_prefix" ; then
        ac_boost_prefix="/usr"
        AC_MSG_RESULT([prefix: $ac_boost_prefix])
    fi

    if test -d "$ac_boost_prefix/lib64" ; then
      TMPVAR=`ls $ac_boost_prefix/lib64/*boost*.so`
      if [ "x${TMPVAR}" == "x" ] ; then
        libdir_local="lib"
      else
        libdir_local="lib64"
      fi
    else
      libdir_local="lib"
    fi

    ac_boost_libraries="$ac_boost_prefix/$libdir_local"
    ac_boost_includes="$ac_boost_prefix/include"
    unset ac_boost_flavour

    if test "$ac_boost_libraries" != "/usr/$libdir_local" ; then

        if test $ac_boost_debug = yes; then
          ac_boost_flavor=debug
        else
          ac_boost_flavor=release
        fi
        ac_boost_libraries="$ac_boost_prefix/$libdir_local/$ac_boost_flavor"

        BOOST_LIBS="-L$ac_boost_libraries $BOOST_LIBS"
        BOOST_FS_LIBS="-L$ac_boost_libraries $BOOST_FS_LIBS"
        BOOST_DATE_LIBS="-L$ac_boost_libraries $BOOST_DATE_LIBS"
        BOOST_THREAD_LIBS="-L$ac_boost_libraries $BOOST_THREAD_LIBS"
        BOOST_REGEX_LIBS="-L$ac_boost_libraries $BOOST_REGEX_LIBS"
	    BOOST_PO_LIBS="-L$ac_boost_libraries $BOOST_PO_LIBS"
        BOOST_PYTHON_LIBS="-L$ac_boost_libraries $BOOST_PYTHON_LIBS"
    fi

    if test "$ac_boost_includes" != "/usr/include" ; then
        BOOST_CFLAGS="-I$ac_boost_includes"
    fi

    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    ac_save_cppflags=$CPPFLAGS
    ac_save_libs=$LIBS
    CPPFLAGS="$BOOST_CFLAGS $CPPFLAGS"
    LIBS="$BOOST_LIBS $LIBS"
    AC_TRY_LINK([ #include <boost/thread/mutex.hpp> ],
                [ boost::mutex   mut; ],
                [ ac_have_boost=yes ], [ ac_have_boost=no ])
    AC_MSG_RESULT([$ac_have_boost])
    CPPFLAGS=$ac_save_cppflags
    LIBS=$ac_save_libs
    AC_LANG_RESTORE

    if test x$ac_have_boost = xyes ; then

        dnl
        dnl Check if the compiler define the stringstream object
        dnl in order to define the BOOST_NO_STRINGSTREAM macro
        dnl 

        AC_LANG_SAVE
        AC_LANG_CPLUSPLUS
        ac_save_cppflags=$CPPFLAGS
        ac_save_libs=$LIBS
        CPPFLAGS="$BOOST_CFLAGS $CPPFLAGS"
        LIBS="$BOOST_LIBS $LIBS"
        AC_MSG_CHECKING([for stringstream into c++ STL])
        AC_TRY_LINK([ #include <sstream> ],
                    [ std::stringstream sstr; ],
                    [ ac_have_stringstream=yes ], 
                    [ ac_have_stringstream=no 
                      BOOST_CFLAGS="$BOOST_CFLAGS -DBOOST_NO_STRINGSTREAM" ])
        if test x$ac_have_stringstream = xyes ; then
            AC_DEFINE([HAVE_STRINGSTREAM], 1, [Define when we are sure to have the right strstream header])
        else 
            AC_DEFINE([BOOST_NO_STRINGSTREAM], 1, [Define when to remove the stringstream use from the boost library])
        fi 
        AC_MSG_RESULT([$ac_have_stringstream])
        CPPFLAGS=$ac_save_cppflags
        LIBS=$ac_save_libs
        AC_LANG_RESTORE

        BOOST_INSTALL_PATH=$ac_boost_prefix

        ifelse([$2], , :, [$2])
        else    
        AC_MSG_WARN([
            ***   Cannot compile a small boost program: check wheter the boost
            ***   libraries are fully installed and try again.])
        BOOST_CFLAGS=""
        BOOST_LIBS=""
        BOOST_FS_LIBS=""
        BOOST_THREAD_LIBS=""
        BOOST_REGEX_LIBS=""
	BOOST_PO_LIBS=""
        BOOST_DATE_LIBS=""
        BOOST_PYTHON_LIBS=""
        ifelse([$3], , :, [$3])
    fi

  else

    dnl see http://www.boost.org/more/getting_started.html#Results
    dnl for an explanation on how the library name is built

    if test -d "$ac_boost_prefix/lib64" ; then
      libdir_local="lib64"
    else
      libdir_local="lib"
    fi

    runtime=
    static_runtime=
    if test $ac_boost_debug = yes; then
      runtime="d"
      static_runtime="d"
    fi
    threading=
    mt_cflags=
    if test x$ac_boost_mt = xyes; then
      threading="mt"
      if test $host_cpu == ia64 ; then
        mt_cflags="-pthread -D_REENTRANT"
      elif test $host_cpu == x86_64 ; then
        mt_cflags="-pthread -D_REENTRANT"
      else
        mt_cflags="-pthread"
      fi
    fi
    toolset="-gcc"
    if test x${threading} != x ; then
      threading="-${threading}"
    fi
    static_runtime="-s${static_runtime}"
    if test x${runtime} != x ; then
      runtime="-${runtime}"
    fi
    ext="${toolset}${threading}${runtime}"
    static_ext="${toolset}${threading}${static_runtime}"

    dnl Test for existance of Boost-style library tags.
    if test ! -r $ac_boost_prefix/$libdir_local/libboost_regex$ext.so  -a \
              -r $ac_boost_prefix/$libdir_local/libboost_regex.so ; then
      AC_MSG_WARN([*** Cannot find Boost libraries tagged with $ext. Building with no library tag.])
      ext=
      static_ext=
    fi
 
    unset runtime static_runtime threading toolset

    BOOST_CFLAGS="$mt_cflags -I$ac_boost_prefix/include"

    BOOST_FS_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_filesystem$ext"
    BOOST_THREAD_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_thread$ext -lpthread"
    BOOST_REGEX_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_regex$ext"
    BOOST_PO_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_program_options$ext"
    BOOST_PYTHON_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_python$ext"
    BOOST_DATE_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_date_time$ext"
    BOOST_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_filesystem$ext -lboost_thread$ext -lpthread -lboost_regex$ext -lboost_program_options$ext -lboost_date_time$ext"

    BOOST_THREAD_STATIC_LIBS="-L$ac_boost_prefix/$libdir_local -lboost_thread${static_ext} -lpthread"

    BOOST_INSTALL_PATH=$ac_boost_prefix
    unset mt_cflags ext static_ext

    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    ac_save_cppflags=$CPPFLAGS
    ac_save_libs=$LIBS
    CPPFLAGS="$BOOST_CFLAGS $CPPFLAGS"
    LIBS="$BOOST_LIBS $LIBS"
    AC_TRY_LINK([ #include <boost/thread/mutex.hpp> ],
                [ boost::mutex   mut; ],
                [ ac_have_boost=yes ], [ ac_have_boost=no ])
    AC_MSG_RESULT([$ac_have_boost])
    CPPFLAGS=$ac_save_cppflags
    LIBS=$ac_save_libs
    AC_LANG_RESTORE

    dnl assume sstream available
    AC_DEFINE([HAVE_STRINGSTREAM], 1, [Define when we are sure to have the right strstream header])

    if test x$ac_have_boost = xyes ; then
        ifelse([$2], , :, [$2])
    else    
        ifelse([$3], , :, [$3])
    fi

  fi

  AC_SUBST(BOOST_INSTALL_PATH)
  AC_SUBST(BOOST_CFLAGS)
  AC_SUBST(BOOST_LIBS)
  AC_SUBST(BOOST_DATE_LIBS)
  AC_SUBST(BOOST_FS_LIBS)
  AC_SUBST(BOOST_THREAD_LIBS)
  AC_SUBST(BOOST_THREAD_STATIC_LIBS)
  AC_SUBST(BOOST_REGEX_LIBS)
  AC_SUBST(BOOST_PO_LIBS)
  AC_SUBST(BOOST_PYTHON_LIBS)
])



dnl Usage:
dnl AC_GLITE
dnl - GLITE_LOCATION
dnl - GLITE_CFLAGS
dnl - GLITE_LDFLAGS
dnl - DISTTAR

AC_DEFUN([AC_GLITE],
[
    AC_ARG_WITH(glite_location,
        [  --with-glite-location=PFX     prefix where GLITE is installed. (/opt/glite)],
        [],
        with_glite_location=/opt/glite)

    if test -n "with_glite_location" ; then
    	GLITE_LOCATION="$with_glite_location"
	GLITE_CFLAGS="-I$GLITE_LOCATION/include"
    else
	GLITE_LOCATION=""
	GLITE_CFLAGS=""
    fi

    AC_MSG_RESULT([GLITE_LOCATION set to $GLITE_LOCATION])

    AC_SUBST(GLITE_LOCATION)
    AC_SUBST(GLITE_CFLAGS)

    if test "x$host_cpu" = "xx86_64"; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib64"
    else
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    AC_SUBST(GLITE_LDFLAGS)

    AC_ARG_WITH(dist_location,
        [  --with-dist-location=PFX     prefix where DIST location is. (pwd)],
        [],
        with_dist_location=$WORKDIR/../dist)

    DISTTAR=$with_dist_location

    AC_SUBST(DISTTAR)

    if test "x$host_cpu" = "xx86_64"; then
        AC_SUBST([libdir], ['${exec_prefix}/lib64'])
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    AC_SUBST([mandir], ['${prefix}/share/man'])
])

AC_DEFUN([GLITE_CHECK_LIBDIR],
[
    if test "x$host_cpu" = "xx86_64"; then
        AC_SUBST([libdir], ['${exec_prefix}/lib64'])
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    AC_SUBST([mandir], ['${prefix}/share/man'])

    AC_ARG_VAR(PVER, Override the version string)
    AC_SUBST([PVER], [${PVER:-$VERSION}])

])

AC_DEFUN([GLITE_CHECK_INITDIR],
[
    if test -d /etc/rc.d -a -d /etc/rc.d/init.d ; then
        AC_SUBST([initdir], [rc.d/init.d])
    else
        AC_SUBST([initdir], [init.d])
    fi
])


# GLITE_BASE
# check that the gLite location directory is present
# if so, set GLITE_LOCATION, GLITE_CPPFLAGS and GLITE_LDFLAGS
# if the host is an x86_64 reset libdir
# reset mandir to comply with FHS
AC_DEFUN([GLITE_BASE],
[AC_ARG_WITH(
    [glite_location],
    [AS_HELP_STRING(
       [--with-glite-location=PFX],
       [prefix where gLite is installed @<:@default=/opt/glite@:>@]
    )],
    [],
    [with_glite_location=/opt/glite]
)
AC_MSG_CHECKING([for gLite location])
if test -d "$with_glite_location"; then
    GLITE_LOCATION="$with_glite_location"
    GLITE_CPPFLAGS="-I $GLITE_LOCATION/include"
    if test "x$host_cpu" = "xx86_64"; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib64"
    else
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi
    AC_MSG_RESULT([$with_glite_location])
else
    AC_MSG_ERROR([$with_glite_location: no such directory])
fi

AC_SUBST([GLITE_LOCATION])
AC_SUBST([GLITE_CPPFLAGS])
AC_SUBST([GLITE_LDFLAGS])

if test "x$host_cpu" = "xx86_64"; then
    AC_SUBST([libdir], ['${exec_prefix}/lib64'])
fi

# added to handle Debian 4/5
if test -h "/usr/lib64" ; then
    AC_SUBST([libdir], ['${exec_prefix}/lib'])
fi

# added to handle platforms that don't use lib64 at all
if ! test -e "/usr/lib64" ; then
    AC_SUBST([libdir], ['${exec_prefix}/lib'])
fi

AC_SUBST([mandir], ['${prefix}/share/man'])
])

dnl Usage:
dnl AC_GLITE_CEMONITOR_API
dnl - GLITE_CEMONITOR_CFLAGS
dnl - GLITE_CEMONITOR_LIBS

dnl Usage:
dnl AC_GLITE_CREAM_API
dnl - GLITE_CREAM_CFLAGS
dnl - GLITE_CREAM_LIBS

dnl Usage:
dnl AC_GLITE_CE_WSDL
dnl - GLITE_CREAM_WSDL
dnl - GLITE_CREAM_WSDL_DEPS
dnl - GLITE_CEMON_CLIENT_WSDL
dnl - GLITE_CEMON_CLIENT_WSDL_DEPS
dnl - GLITE_CEMON_CONSUMER_WSDL
dnl - GLITE_CEMON_CONSUMER_WSDL_DEPS

dnl Usage:
dnl AC_GLITE_ES_WSDL
dnl - GLITE_ES_WSDL
dnl - GLITE_ES_WSDL_DEPS

AC_DEFUN([AC_GLITE_CEMONITOR_API],
[
    ac_glite_ce_prefix=$GLITE_LOCATION

    have_cemon_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_CEMONITOR, monitor-client-api-c, have_cemon_pkgconfig=yes, have_cemon_pkgconfig=no)

    if test "x$have_cemon_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects CEMonitor definitions: $GLITE_CEMONITOR_CFLAGS $GLITE_CEMONITOR_LIBS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_ce_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib64"
        else
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib"
        fi

	GLITE_CEMONITOR_LIBS="$ac_glite_ce_lib -lglite_ce_monitor_client -lglite_ce_monitor_consumer"
        GLITE_CEMONITOR_CFLAGS="-I$ac_glite_ce_prefix/include -I$ac_glite_ce_prefix/include/glite/ce"
	ifelse([$2], , :, [$2])
    else
        GLITE_CEMONITOR_LIBS=""
	GLITE_CEMONITOR_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_CEMONITOR_CFLAGS)
    AC_SUBST(GLITE_CEMONITOR_LIBS)
])


AC_DEFUN([AC_GLITE_CREAM_API],
[
    ac_glite_ce_prefix=$GLITE_LOCATION

    have_cream_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_CREAM, cream-client-api-c, have_cream_pkgconfig=yes, have_cream_pkgconfig=no)

    if test "x$have_cream_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects CREAM definitions: $GLITE_CREAM_CFLAGS $GLITE_CREAM_LIBS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_ce_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib64"
        else
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib"
        fi

        GLITE_CREAM_LIBS="$ac_glite_ce_lib -lglite_ce_cream_client_soap -lglite_ce_cream_client_util"
        GLITE_CREAM_CFLAGS="-I$ac_glite_ce_prefix/include -I$ac_glite_ce_prefix/include/glite/ce"
        ifelse([$2], , :, [$2])
    else
        GLITE_CREAM_LIBS=""
        GLITE_CREAM_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_CREAM_CFLAGS)
    AC_SUBST(GLITE_CREAM_LIBS)
])



AC_DEFUN([AC_GLITE_CE_WSDL],
[

    AC_ARG_WITH(ce_wsdl_location,
        [  --with-ce-wsdl-location=PATH    path of the wsdl file set],
        [],
        with_ce_wsdl_location=${GLITE_LOCATION:-/usr})

    GLITE_CE_WSDL_PATH=$with_ce_wsdl_location/share/wsdl/cream-ce
    AC_MSG_CHECKING([CREAM CE WSDL files in $with_ce_wsdl_location/share/wsdl/cream-ce])

    GLITE_CREAM_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-cream2_service.wsdl
    GLITE_CREAM_WSDL_DEPS="$GLITE_CREAM_WSDL \
                           $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd \
                           $GLITE_CE_WSDL_PATH/www.gridsite.org-delegation-2.0.0.wsdl"

    GLITE_CEMON_CLIENT_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-monitor_service.wsdl
    GLITE_CEMON_CLIENT_WSDL_DEPS="$GLITE_CEMON_CLIENT_WSDL \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_types.wsdl \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_faults.wsdl \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd"

    GLITE_CEMON_CONSUMER_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-monitor_consumer_service.wsdl
    GLITE_CEMON_CONSUMER_WSDL_DEPS="$GLITE_CEMON_CONSUMER_WSDL \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_types.wsdl \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_faults.wsdl \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd"

    if test -f "$GLITE_CREAM_WSDL" -a -f "$GLITE_CEMON_CLIENT_WSDL" ; then
        AC_SUBST(GLITE_CE_WSDL_PATH)
        AC_SUBST(GLITE_CREAM_WSDL)
        AC_SUBST(GLITE_CREAM_WSDL_DEPS)
        AC_SUBST(GLITE_CEMON_CLIENT_WSDL)
        AC_SUBST(GLITE_CEMON_CLIENT_WSDL_DEPS)
        AC_SUBST(GLITE_CEMON_CONSUMER_WSDL)
        AC_SUBST(GLITE_CEMON_CONSUMER_WSDL_DEPS)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([no])])
    fi
])




AC_DEFUN([AC_GLITE_ES_WSDL],
[

    AC_ARG_WITH(es_wsdl_location,
        [  --with-es-wsdl-location=PATH    path of the wsdl file set],
        [],
        with_es_wsdl_location=${GLITE_LOCATION:-/usr})

    GLITE_ES_WSDL_PATH=$with_es_wsdl_location/share/wsdl/cream-ce/es
    AC_MSG_CHECKING([ES CE WSDL files in $with_es_wsdl_location/share/wsdl/cream-ce/es])

    GLITE_ES_WSDL="$GLITE_ES_WSDL_PATH/Creation.wsdl \
			$GLITE_ES_WSDL_PATH/ActivityManagement.wsdl \
			$GLITE_ES_WSDL_PATH/ActivityInfo.wsdl \
			$GLITE_ES_WSDL_PATH/Delegation.wsdl \
			$GLITE_ES_WSDL_PATH/ResourceInfo.wsdl"

    GLITE_ES_WSDL_DEPS="$GLITE_ES_WSDL \
			$GLITE_ES_WSDL_PATH/ActivityInfo.xsd \
			$GLITE_ES_WSDL_PATH/ActivityManagement.xsd \
			$GLITE_ES_WSDL_PATH/Creation.xsd \
			$GLITE_ES_WSDL_PATH/Delegation.xsd \
			$GLITE_ES_WSDL_PATH/es-activity-description.xsd \
			$GLITE_ES_WSDL_PATH/es-main.xsd \
			$GLITE_ES_WSDL_PATH/GLUE2.xsd \
			$GLITE_ES_WSDL_PATH/ResourceInfo.xsd"

    file_missing=true
    for item in $GLITE_ES_WSDL_DEPS ; do
        if test -f $item ; then
            file_missing=false
        fi
    done

    if test "x$file_missing" == "xfalse"; then
        AC_SUBST(GLITE_ES_WSDL_PATH)
        AC_SUBST(GLITE_ES_WSDL)
        AC_SUBST(GLITE_ES_WSDL_DEPS)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([no])])
    fi
])



dnl
dnl Define GSOAP
dnl
AC_DEFUN([AC_GSOAP],
[
	dnl
	dnl GSOAP Location
	dnl
	AC_ARG_WITH(gsoap-location,
	[  --with-gsoap-location=PFX     prefix where GSOAP is installed. (/usr)],
	[],
        with_gsoap_location=${GSOAP_LOCATION:-/usr})

    if test "x$host_cpu" = "xx86_64"; then
        ac_gsoap_ldlib="$with_gsoap_location/lib64"

        if ! test -e "ac_gsoap_ldlib" -o -h "ac_gsoap_ldlib" ; then
            ac_gsoap_ldlib="$with_gsoap_location/lib"
        fi

    else
        ac_gsoap_ldlib="$with_gsoap_location/lib"
    fi

	AC_MSG_RESULT([checking for gsoap... ])

	if test -n "$with_gsoap_location" ; then
		GSOAP_LOCATION="$with_gsoap_location"
		GSOAP_CFLAGS="-I$with_gsoap_location/include -I$with_gsoap_location -I$with_gsoap_location/extras"
dnl		GSOAP_LIBS="-L$ac_gsoap_ldlib -lgsoap -lgsoapssl -lgsoapck"
dnl		GSOAPXX_LIBS="-L$ac_gsoap_ldlib -lgsoap++ -lgsoapssl++ -lgsoapck++"
dnl		GSOAP_STATIC_LIBS="$ac_gsoap_ldlib/libgsoap.a $ac_gsoap_ldlib/libgsoapck.a $ac_gsoap_ldlib/libgsoapssl.a"
                GSOAP_LIBS="-L$ac_gsoap_ldlib -lgsoapssl"
                GSOAPXX_LIBS="-L$ac_gsoap_ldlib -lgsoapssl++"
                GSOAP_STATIC_LIBS="$ac_gsoap_ldlib/libgsoapssl.a"
    else
		GSOAP_LOCATION=""
		GSOAP_CFLAGS=""
		GSOAP_LIBS=""
		GSOAPXX_LIBS=""
		GSOAP_STATIC_LIBS=""
    fi

	dnl
	dnl GSOAP WSDL2H Location
	dnl
	AC_ARG_WITH(gsoap-wsdl2h-location,
	[  --with-gsoap-wsdl2h-location=PFX     prefix where GSOAP wsdl2h is installed. (/usr)],
	[],
        with_gsoap_wsdl2h_location=${GSOAP_WSDL2H_LOCATION:-$GSOAP_LOCATION})

	AC_MSG_RESULT([checking for gsoap wsdl2h... ])

	if test -n "$with_gsoap_wsdl2h_location" ; then
		GSOAP_WSDL2H_LOCATION="$with_gsoap_wsdl2h_location"
    else
		GSOAP_WSDL2H_LOCATION="$GSOAP_LOCATION"
    fi

	dnl
	dnl GSOAP Version
	dnl
	AC_ARG_WITH(gsoap-version,
	[  --with-gsoap-version=PFX     GSOAP version (2.3.8)],
	[],
        with_gsoap_version=${GSOAP_VERSION:-2.3.8})

	AC_MSG_RESULT([checking for gsoap version... ])
	
	if test -n "$with_gsoap_version" ; then
		GSOAP_VERSION="$with_gsoap_version"
    else
		GSOAP_VERSION=""
    fi
    
    dnl
	dnl GSOAP WSDL2H Version
	dnl
	AC_ARG_WITH(gsoap-wsdl2h-version,
	[  --with-gsoap-wsdl2h-version=PFX     GSOAP WSDL2h version (2.3.8)],
	[],
        with_gsoap_wsdl2h_version=${GSOAP_WSDL2H_VERSIONS:-$GSOAP_VERSION})

	AC_MSG_RESULT([checking for gsoap WSDL2H version... ])
	
	if test -n "$with_gsoap_wsdl2h_version" ; then
		GSOAP_WSDL2H_VERSION="$with_gsoap_wsdl2h_version"
    else
		GSOAP_WSDL2H_VERSION="$GSOAP_VERSION"
    fi
    
    dnl
	dnl Set GSOAP Version Number as a compiler Definition
	dnl
	if test -n "$GSOAP_VERSION" ; then
		EXPR='foreach $n(split(/\./,"'$GSOAP_VERSION'")){if(int($n)>99){$n=99;}printf "%02d",int($n);} print "\n";'
		GSOAP_VERSION_NUM=`perl -e "$EXPR"`
	else
		GSOAP_VERSION_NUM=000000
	fi;		
	GSOAP_CFLAGS="$GSOAP_CFLAGS -D_GSOAP_VERSION=0x$GSOAP_VERSION_NUM"

	dnl
	dnl Set GSOAP WSDL2H Version Number as a compiler Definition
	dnl
	if test -n "$GSOAP_WSDL2H_VERSION" ; then
		EXPR='foreach $n(split(/\./,"'$GSOAP_WSDL2H_VERSION'")){if(int($n)>99){$n=99;}printf "%02d",int($n);} print "\n";'
		GSOAP_WSDL2H_VERSION_NUM=`perl -e "$EXPR"`
	else
		GSOAP_WSDL2H_VERSION_NUM=000000
	fi;		
	GSOAP_CFLAGS="$GSOAP_CFLAGS -D_GSOAP_WSDL2H_VERSION=0x$GSOAP_WSDL2H_VERSION_NUM"

    AC_MSG_RESULT([GSOAP_LOCATION set to $GSOAP_LOCATION])
	AC_MSG_RESULT([GSOAP_WSDL2H_LOCATION set to $GSOAP_WSDL2H_LOCATION])
    AC_MSG_RESULT([GSOAP_CFLAGS set to $GSOAP_CFLAGS])
    AC_MSG_RESULT([GSOAP_LIBS set to $GSOAP_LIBS])
    AC_MSG_RESULT([GSOAP_STATIC_LIBS set to $GSOAP_STATIC_LIBS])
    AC_MSG_RESULT([GSOAP_VERSION set to $GSOAP_VERSION])
    AC_MSG_RESULT([GSOAP_WSDL2H_VERSION set to $GSOAP_WSDL2H_VERSION])

	AC_SUBST(GSOAP_LOCATION)
	AC_SUBST(GSOAP_WSDL2H_LOCATION)
	AC_SUBST(GSOAP_CFLAGS)
	AC_SUBST(GSOAP_LIBS)
	AC_SUBST(GSOAPXX_LIBS)
	AC_SUBST(GSOAP_STATIC_LIBS)
    AC_SUBST(GSOAP_VERSION)
    AC_SUBST(GSOAP_VERSION_NUM)
	AC_SUBST(GSOAP_WSDL2H_VERSION)
    AC_SUBST(GSOAP_WSDL2H_VERSION_NUM)
    
    dnl
	dnl Test GSOAP Version
	dnl
	if test [ "$GSOAP_VERSION_NUM" -ge "020300" -a "$GSOAP_VERSION_NUM" -lt "020600"] ; then
    	AC_MSG_RESULT([GSOAP_VERSION is 2.3])
		GSOAP_MAIN_VERSION=`expr substr "$GSOAP_VERSION" 1 3`
		SOAPCPP2="$GSOAP_LOCATION/bin/soapcpp2"
		SOAPCPP2_FLAGS="-n"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAP_2_3"
	elif test [ "$GSOAP_VERSION_NUM" -ge "020600"] ; then
    	AC_MSG_RESULT([GSOAP_VERSION is 2.6 or newer])
		GSOAP_MAIN_VERSION=`expr substr "$GSOAP_VERSION" 1 3`
		SOAPCPP2="$GSOAP_LOCATION/bin/soapcpp2"
		SOAPCPP2_FLAGS="-n -w"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAP_2_6"
	else
    	AC_MSG_RESULT([Unsupported GSOAP_VERSION])
		GSOAP_MAIN_VERSION=""
		SOAPCPP2=""
		SOAPCPP2_FLAGS=""
		GSOAP_CFLAGS=""
	fi
	
	dnl
	dnl Test GSOAP WSDL2H Version
	dnl
	if test [ "$GSOAP_WSDL2H_VERSION_NUM" -ge "020300" -a "$GSOAP_WSDL2H_VERSION_NUM" -lt "020600"] ; then
		AC_MSG_RESULT([GSOAP_WSDL2H_VERSION is 2.3])
	    	
	    dnl
		dnl Check Xerces Location
		dnl
		AC_MSG_RESULT([checking for xerces (neeeded by GSOAP 2.3)... ])
		
		AC_ARG_WITH(xerces-location,
				[  --with-xerces-location=PFX     prefix where Xerces is installed. (/usr/share/java/xerces)],
				[],
		    	    with_xerces_location=${XERCES_LOCATION:-/usr/share/java/xerces})
		
		if test -n "$with-xerces-location" ; then
			XERCES_LOCATION="$with_xerces_location"
		else
			XERCES_LOCATION=""
		fi
		AC_MSG_RESULT([XERCES_LOCATION set to $XERCES_LOCATION])
		
		AC_SUBST(XERCES_LOCATION)

		GSOAP_WSDL2H_MAIN_VERSION=`expr substr "$GSOAP_WSDL2H_VERSION" 1 3`
		WSDL2H="java -classpath $GSOAP_WSDL2H_LOCATION/share/wsdlcpp.jar:$XERCES_LOCATION/xmlParserAPIs.jar:$XERCES_LOCATION/xercesImpl.jar wsdlcpp"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAPWSDL2H_2_3"
	elif test [ "$GSOAP_WSDL2H_VERSION_NUM" -ge "020600"] ; then
    	AC_MSG_RESULT([GSOAP_WSDL2H_VERSION is 2.6 or newer])
		GSOAP_WSDL2H_MAIN_VERSION=`expr substr "$GSOAP_WSDL2H_VERSION" 1 3`
		WSDL2H="$GSOAP_WSDL2H_LOCATION/bin/wsdl2h"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAPWSDL2H_2_6"
	else
    	AC_MSG_RESULT([Unsupported GSOAP_WSDL2H_VERSION])
		GSOAP_WSDL2H_MAIN_VERSION=""
		WSDL2H=""
	fi

	dnl
	dnl Obsolete !? Should be removed !?
	dnl
    AM_CONDITIONAL(USEGSOAP_2_3, 	   [test x$GSOAP_MAIN_VERSION = x2.3])
	AM_CONDITIONAL(USEGSOAP_2_6, 	   [test x$GSOAP_MAIN_VERSION = x2.6])
	AM_CONDITIONAL(USEGSOAPWSDL2H_2_3, [test x$GSOAP_WSDL2H_MAIN_VERSION = x2.3])
	AM_CONDITIONAL(USEGSOAPWSDL2H_2_6, [test x$GSOAP_WSDL2H_MAIN_VERSION = x2.6])

	AC_SUBST(GSOAP_CFLAGS)
	AC_SUBST(WSDL2H)
	AC_SUBST(SOAPCPP2)
	AC_SUBST(SOAPCPP2_FLAGS)
	AC_SUBST(GSOAP_MAIN_VERSION)
	AC_SUBST(GSOAP_WSDL2H_MAIN_VERSION)
])


AC_DEFUN([AC_GSOAP_EXTRAS],
[
    AC_ARG_WITH(gsoap-location,
    [  --with-gsoap-location=PFX     prefix where GSOAP is installed. (/usr)],
    [],
    with_gsoap_location=${GSOAP_LOCATION:-/usr})

    GSOAP_LOCATION="$with_gsoap_location"

    GSOAP_VERSION=`${GSOAP_LOCATION}/bin/soapcpp2 -v 2>&1 | grep -Po "\d+\.\d+\.\d+"`
    AC_MSG_RESULT([Detected gsoap version ${GSOAP_VERSION}])

    if test -n "$GSOAP_VERSION" ; then
        dnl EXPR='foreach $n(split(/\./,"'$GSOAP_VERSION'")){if(int($n)>99){$n=99;}printf "%02d",int($n);} print "\n";'
        dnl GSOAP_VERSION_NUM=`perl -e "$EXPR"`
        GSOAP_VERSION_NUM=`echo $GSOAP_VERSION | awk -F . '{print $'1'*10000+$'2'*100+$'3'}'`
    else
        GSOAP_VERSION_NUM=000000
    fi
    
    if test ${GSOAP_VERSION_NUM} -ge 20716 ; then
        GSOAP_WSDL2H_OPTS="-k -z1 -z2"
    elif test ${GSOAP_VERSION_NUM} -ge 20713 ; then
        GSOAP_WSDL2H_OPTS="-k"
    else
        GSOAP_WSDL2H_OPTS=""
    fi 

    AC_SUBST(GSOAP_VERSION_NUM)
    AC_SUBST(GSOAP_LOCATION)
    AC_SUBST(GSOAP_WSDL2H_OPTS)
])

AC_DEFUN([AC_OPTIMIZE],
[
    AC_MSG_CHECKING([for OPTIMIZATION parameters])

    AC_ARG_ENABLE(optimization,
	[  --enable-optimization Enable optimization],
        [ if test "$enableval" = "yes"
          then
            optimizopt=$enableval
            if test "$GCC" = "yes"
            then
              optimise='-O2 '
            else
              optimise='-O '
            fi
          else
            optimizopt=$enableval
            if test "$GCC" = "yes"
            then
              optimise='-O0 '
            else
              optimise=''
            fi
         fi],
         optimizopt=$enableval
    )
  
    if test "x$optimizopt" = "xyes" ; then
        AC_MSG_RESULT(yes)
    else
        AC_MSG_RESULT(no)
    fi
  
    if test "x$optimizopt" = "xyes" ; then
        CXXFLAGS="-g -Wall $optimise"
        CFLAGS="-g -Wall $optimise"
    else
        CXXFLAGS="-g -Wall $optimise"
        CFLAGS="-g -Wall $optimise"
    fi

    AC_SUBST(CXXFLAGS)
    AC_SUBST(CFLAGS)
])


