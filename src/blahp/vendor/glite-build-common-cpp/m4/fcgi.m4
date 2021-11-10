dnl Usage:
dnl AC_FCGI(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for expat, and defines
dnl - FCGI_CFLAGS (compiler flags)
dnl - FCGI_LIBS (linker flags, stripping and path)
dnl - FCGI_STATIC_LIBS (linker flags, stripping and path for static library)
dnl - FCGI_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_FCGI],
[
    AC_ARG_WITH(fcgi_prefix, 
	[  --with-fcgi-prefix=PFX      prefix where 'fcgi' is installed.],
	[], 
        with_fcgi_prefix=${FCGI_INSTALL_PATH:-/usr})

    AC_MSG_CHECKING([for FCGI installation at ${with_fcgi_prefix:-/usr}])

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS
    if test -n "$with_fcgi_prefix" -a "$with_fcgi_prefix" != "/usr" ; then
	FCGI_CFLAGS="-I$with_fcgi_prefix/include"
	if test "x$host_cpu" = "xx86_64"; then
            FCGI_CPP_LIBS="-L$with_fcgi_prefix/lib64"
        else
            FCGI_CPP_LIBS="-L$with_fcgi_prefix/lib"
        fi
    else
	FCGI_CFLAGS=""
	FCGI_CPP_LIBS=""
    fi

    FCGI_CPP_LIBS="$FCGI_CPP_LIBS -lfcgi++"	
    CFLAGS="$FCGI_CFLAGS $CFLAGS"
    LIBS="$FCGI_CPP_LIBS $LIBS"
    AC_TRY_COMPILE([ #include <fcgi_stdio.h> ],
    		   [ FCGI_FILE fcgi_test ],
    		   [ ac_cv_fcgi_valid=yes ], [ ac_cv_fcgi_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS	
    AC_MSG_RESULT([$ac_cv_fcgi_valid])

    if test x$ac_cv_fcgi_valid = xyes ; then
        FCGI_INSTALL_PATH=$with_expat_prefix
	ifelse([$2], , :, [$2])
    else
	FCGI_CFLAGS=""
	FCGI_CPP_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(FCGI_INSTALL_PATH)
    AC_SUBST(FCGI_CFLAGS)
    AC_SUBST(FCGI_CPP_LIBS)
])

