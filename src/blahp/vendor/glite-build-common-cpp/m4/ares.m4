dnl Usage:
dnl AC_ARES(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for ares, and defines
dnl - ARES_CFLAGS (compiler flags)
dnl - ARES_LIBS (linker flags, stripping and path)
dnl - ARES_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_ARES],
[
    AC_ARG_WITH(ares_prefix,
        [  --with-ares-prefix=PFX       prefix where 'ares' is installed.],
        [],
        with_ares_prefix=${ARES_INSTALL_PATH:-/usr})

    have_ares=no
    PKG_CHECK_MODULES(ARES, libcares, have_ares=yes, have_ares=no)
    
    if test "x$have_ares" = "xyes" ; then
	ARES_INSTALL_PATH=""
        ifelse([$2], , :, [$2])
    else

    	AC_MSG_CHECKING([for ARES installation at ${with_ares_prefix} on ${host_cpu}])

    	if test $host_cpu = x86_64 -o "$host_cpu" = ia64 ; then
        	ac_ares_lib_dir="lib64"
    	else
        	ac_ares_lib_dir="lib"
    	fi

    	ac_save_CFLAGS=$CFLAGS
    	ac_save_LIBS=$LIBS
    	if test -n "$with_ares_prefix" ; then
        	ARES_CFLAGS="-I$with_ares_prefix/include"
        	ARES_LIBS="-L$with_ares_prefix/$ac_ares_lib_dir"
    	else
        	ARES_CFLAGS=""
        	ARES_LIBS=""
    	fi

    	ARES_LIBS="$ARES_LIBS -lcares"
    	CFLAGS="$ARES_CFLAGS $CFLAGS"
    	LIBS="$ARES_LIBS $LIBS"
    	AC_TRY_COMPILE([ #include "ares.h" ],
                       [ ],
                       [ ac_cv_ares_valid=yes ], [ ac_cv_ares_valid=no ])
    	CFLAGS=$ac_save_CFLAGS
    	LIBS=$ac_save_LIBS
    	AC_MSG_RESULT([$ac_cv_ares_valid])

    	if test x$ac_cv_ares_valid = xyes ; then
        	ARES_INSTALL_PATH=$with_ares_prefix
        	ifelse([$2], , :, [$2])
    	else
        	ARES_CFLAGS=""
        	ARES_LIBS=""
        	ifelse([$3], , :, [$3])
    	fi
    fi

    AC_SUBST(ARES_INSTALL_PATH)    
    AC_SUBST(ARES_CFLAGS)
    AC_SUBST(ARES_LIBS)
])

