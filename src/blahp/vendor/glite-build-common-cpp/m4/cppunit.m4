dnl Usage:
dnl AC_CPPUNIT(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for cppunit, and defines
dnl - CPPUNIT_CFLAGS (compiler flags)
dnl - CPPUNIT_LIBS (linker flags, stripping and path)
dnl prerequisites:

AC_DEFUN([AC_CPPUNIT],
[
    AC_ARG_WITH(cppunit_prefix, 
	[  --with-cppunit-prefix=PFX prefix where 'cppunit' is installed.],
	[], 
        with_cppunit_prefix=${CPPUNIT_INSTALL_PATH})

    AC_MSG_CHECKING([for CPPUNIT installation at ${with_cppunit_prefix:-/usr}])

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS
    if test -n "$with_cppunit_prefix" -a "$with_cppunit_prefix" != "/usr" ; then
	CPPUNIT_CFLAGS="-I$with_cppunit_prefix/include"
	CPPUNIT_LIBS="-L$with_cppunit_prefix/lib"
    else
	CPPUNIT_CFLAGS=""
	CPPUNIT_LIBS=""
    fi

    CPPUNIT_LIBS="$CPPUNIT_LIBS -lcppunit -lpthread -ldl"	
    CFLAGS="$CPPUNIT_CFLAGS $CFLAGS"
    LIBS="$CPPUNIT_LIBS $LIBS"
    AC_TRY_COMPILE([ #include <cppunit/ui/qt/Config.h> ],
    		   [ ],
    		   [ ac_cv_cppunit_valid=yes ], [ ac_cv_cppunit_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS	
    AC_MSG_RESULT([$ac_cv_cppunit_valid])

    if test x$ac_cv_cppunit_valid = xyes ; then
	ifelse([$2], , :, [$2])
    else
	CPPUNIT_CFLAGS=""
	CPPUNIT_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(CPPUNIT_CFLAGS)
    AC_SUBST(CPPUNIT_LIBS)
])

