dnl Usage:
dnl AC_DB(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for db, and defines
dnl - DB_CFLAGS (compiler flags)
dnl - DB_LIBS (linker flags, stripping and path)
dnl - DB_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_DB],
[
    AC_ARG_WITH(db_prefix,
	[  --with-db-prefix=PFX       prefix where 'db' is installed.],
	[], 
        with_db_prefix=${DB_INSTALL_PATH:-/usr})

    AC_MSG_CHECKING([for DB installation at ${with_db_prefix}])

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS
    if test -n "$with_db_prefix" -a "$with_db_prefix" != "/usr" ; then
	DB_CFLAGS="-I$with_db_prefix/include"
	DB_LIBS="-L$with_db_prefix/lib"
    else
	DB_CFLAGS=""
	DB_LIBS=""
    fi

    DB_LIBS="$DB_LIBS -ldb_cxx-4"
    CFLAGS="$DB_CFLAGS $CFLAGS"
    LIBS="$DB_LIBS $LIBS"
    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    AC_TRY_COMPILE([ #include "db_cxx.h" ],
    		   [ ],
    		   [ ac_cv_db_valid=yes ], [ ac_cv_db_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS
    AC_LANG_RESTORE
    AC_MSG_RESULT([$ac_cv_db_valid])

    if test x$ac_cv_db_valid = xyes ; then
        DB_INSTALL_PATH=$with_db_prefix
	ifelse([$2], , :, [$2])
    else
	DB_CFLAGS=""
	DB_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(DB_INSTALL_PATH)
    AC_SUBST(DB_CFLAGS)
    AC_SUBST(DB_LIBS)
])


