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


