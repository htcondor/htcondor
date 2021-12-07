dnl Usage:
dnl AC_UNIXODBC(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for unixODBC, and defines
dnl - UNIXODBC_CFLAGS (compiler flags)
dnl - UNIXODBC_LIBS (linker flags, stripping and path)
dnl - UNIXODBC_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_UNIXODBC],
[
    AC_ARG_WITH(unixodbc_prefix,
	[  --with-unixodbc-prefix=PFX       prefix where 'UNIXODBC' is installed.],
	[], 
        with_unixodbc_prefix=${UNIXODBC_INSTALL_PATH:-/usr})

    AC_MSG_CHECKING([for unixODBC installation at ${with_unixodbc_prefix}])

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS
    if test -n "$with_unixodbc_prefix" -a "$with_unixodbc_prefix" != "/usr" ; then
        UNIXODBC_CFLAGS="-I$with_unixodbc_prefix/include"
        if test "x$HOSTTYPE" = "xx86_64"; then
            UNIXODBC_LIBS="-L$with_unixodbc_prefix/lib64"
        else
            UNIXODBC_LIBS="-L$with_unixodbc_prefix/lib"
        fi
    else
        UNIXODBC_CFLAGS=""
        UNIXODBC_LIBS=""
    fi

    UNIXODBC_LIBS="$UNIXODBC_LIBS -lodbc -lodbcinst"
    CFLAGS="$UNIXODBC_CFLAGS $CFLAGS"
    LIBS="$UNIXODBC_LIBS $LIBS"
    AC_TRY_LINK([#include<sql.h>],
                [SQLHSTMT st;
                 SQLCancel(st);],
                [ac_cv_unixodbc_valid=yes],
                [ac_cv_unixodbc_valid=no])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS
    AC_MSG_RESULT([$ac_cv_unixodbc_valid])

    if test x$ac_cv_unixodbc_valid = xyes ; then
        UNIXODBC_INSTALL_PATH=$with_unixodbc_prefix
	ifelse([$2], , :, [$2])
    else
        UNIXODBC_CFLAGS=""
        UNIXODBC_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(UNIXODBC_INSTALL_PATH)
    AC_SUBST(UNIXODBC_CFLAGS)
    AC_SUBST(UNIXODBC_LIBS)
])

