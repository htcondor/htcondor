dnl Usage:
dnl AC_HTTPD(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for httpd development tools, and defines
dnl - HTTPD_CFLAGS (compiler flags)
dnl - HTTPD_LIBS (linker flags, stripping and path)
dnl prerequisites:

AC_DEFUN([AC_HTTPD],
[
    AC_ARG_WITH(httpd_prefix, 
	[  --with-httpd-prefix=PFX prefix where 'httpd' is installed.],
	[], 
        with_httpd_prefix=${HTTPD_INSTALL_PATH})

    AC_MSG_CHECKING([for HTTPD installation at ${with_httpd_prefix:-/usr}])

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS
    if test -n "$with_httpd_prefix" -a "$with_httpd_prefix" != "/usr" ; then
	HTTPD_CFLAGS="-I$with_httpd_prefix/include/httpd -I$with_httpd_prefix/include/apr-0 -I$with_httpd_prefix/include/pcre"
	HTTPD_LIBS="-L$with_httpd_prefix/lib"
    else
	HTTPD_CFLAGS=""
	HTTPD_LIBS=""
    fi

    HTTPD_LIBS="$HTTPD_LIBS"	
    CFLAGS="$HTTPD_CFLAGS $CFLAGS"
    LIBS="$HTTPD_LIBS $LIBS"
    AC_TRY_COMPILE([ #include <ap_config.h> ],
    		   [ ],
    		   [ ac_cv_httpd_valid=yes ], [ ac_cv_httpd_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS	
    AC_MSG_RESULT([$ac_cv_httpd_valid])

    if test x$ac_cv_httpd_valid = xyes ; then
	ifelse([$2], , :, [$2])
    else
	HTTPD_CFLAGS=""
	HTTPD_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(HTTPD_CFLAGS)
    AC_SUBST(HTTPD_LIBS)
])

