AC_DEFUN([AC_GLITE_WMS_PURGER],
[
    ac_glite_wms_purger_prefix=$GLITE_LOCATION

    have_purger_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_PURGER, wms-purger, have_purger_pkgconfig=yes, have_purger_pkgconfig=no)

    if test "x$have_purger_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Purger])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_purger_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_purger_lib="-L$ac_glite_wms_purger_prefix/lib64"
        else
            ac_glite_wms_purger_lib="-L$ac_glite_wms_purger_prefix/lib"
        fi

        GLITE_WMS_PURGER_CFLAGS="-I$ac_glite_wms_purger_prefix/include"
        GLITE_WMS_PURGER_LIBS="$ac_glite_wms_purger_lib -lglite_wms_purger"
        ifelse([$2], , :, [$2])
    else
	GLITE_WMS_PURGER_CFLAGS=""
        GLITE_WMS_PURGER_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_PURGER_CFLAGS)
    AC_SUBST(GLITE_WMS_PURGER_LIBS)
])
