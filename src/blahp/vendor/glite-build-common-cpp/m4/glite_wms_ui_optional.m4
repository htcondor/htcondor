dnl Usage:
dnl AC_GLITE_WMS_UI_OPTIONAL

AC_DEFUN([AC_GLITE_WMS_UI_OPTIONAL],
[

    AC_MSG_CHECKING([whether WMS_NS integration is enabled, or not])

    AC_ARG_ENABLE(wms-ns-integration,
        [  --enable-wms-ns-integration=<option> Default is no],
        wmsnsopt="$enableval",
        wmsnsopt="no"
    )

    if test "x$wmsnsopt" = "xno" ; then
        AC_MSG_RESULT([$wmsnsopt])
    else
        AC_MSG_RESULT(yes)
    fi

    AM_CONDITIONAL(DONT_HAVE_NS,[test "x$wmsnsopt" = xno])

])