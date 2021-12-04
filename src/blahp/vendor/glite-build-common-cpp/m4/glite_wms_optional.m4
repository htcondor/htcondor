dnl Usage:
dnl AC_GLITE_WMS_OPTIONAL
dnl GLITE_WMS_OPTIONAL_JP_CFLAGS
dnl GLITE_WMS_OPTIONAL_JP_LIBS
dnl GLITE_WMS_OPTIONAL_GPBOX_CFLAGS
dnl GLITE_WMS_OPTIONAL_GPBOX_LIBS

AC_DEFUN([AC_GLITE_WMS_OPTIONAL],
[
    AC_GLITE_JP
    AC_GLITE_GPBOX

    AC_MSG_CHECKING([whether JP integration is enabled, or not])
    
    AC_ARG_ENABLE(jp-integration,
        [  --enable-jp-integration=<option> Default is no],
        jpinpurgeropt="$enableval",
        jpinpurgeropt="no"
    )

    if test "x$jpinpurgeropt" = "xyes" ; then
        AC_MSG_RESULT([$jpinpurgeropt])
    else
        AC_MSG_RESULT(no)
    fi

    if test "x$jpinpurgeropt" = "xyes" ; then
       GLITE_WMS_OPTIONAL_JP_CFLAGS=""
       GLITE_WMS_OPTIONAL_JP_LIBS="$GLITE_JP_COMMON_LIBS $GLITE_JP_IMPORTER_NOTHR_LIBS $GLITE_JP_TRIO_LIBS $LIBTAR_LIBS"
    else
       GLITE_WMS_OPTIONAL_JP_CFLAGS="-DGLITE_WMS_DONT_HAVE_JP"
       GLITE_WMS_OPTIONAL_JP_LIBS=""
    fi

    AC_SUBST(GLITE_WMS_OPTIONAL_JP_CFLAGS)
    AC_SUBST(GLITE_WMS_OPTIONAL_JP_LIBS)

    AC_MSG_CHECKING([whether GPBOX integration is enabled, or not])
    
    AC_ARG_ENABLE(gpbox-integration,
        [  --enable-gpbox-integration=<option> Default is yes],
        gpboxopt="$enableval",
        gpboxopt="yes"
    )

    if test "x$gpboxopt" = "xyes" ; then
        AC_MSG_RESULT([$gpboxopt])
    else
        AC_MSG_RESULT(no)
    fi

    if test "x$gpboxopt" = "xno" ; then
       GLITE_WMS_OPTIONAL_GPBOX_CFLAGS="-DGLITE_WMS_DONT_HAVE_GPBOX"
       GLITE_WMS_OPTIONAL_GPBOX_LIBS=""
    else
       GLITE_WMS_OPTIONAL_GPBOX_CFLAGS=""
       GLITE_WMS_OPTIONAL_GPBOX_LIBS="$GLITE_GPBOX_LIBS"
    fi
    AC_SUBST(GLITE_WMS_OPTIONAL_GPBOX_CFLAGS)
    AC_SUBST(GLITE_WMS_OPTIONAL_GPBOX_LIBS)
])

