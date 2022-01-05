dnl Usage:
dnl AC_GLITE_WMS_BROKER
dnl - GLITE_WMS_BROKER_CFLAGS
dnl - GLITE_WMS_BROKER_LIBS

AC_DEFUN([AC_GLITE_WMS_BROKER],
[
    ac_glite_wms_broker_prefix=$GLITE_LOCATION

    have_broker_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_BROKER, wms-broker, have_broker_pkgconfig=yes, have_broker_pkgconfig=no)

    if test "x$have_broker_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Broker])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_broker_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_broker_lib="-L$ac_glite_wms_broker_prefix/lib64"
        else
            ac_glite_wms_broker_lib="-L$ac_glite_wms_broker_prefix/lib"
        fi

	GLITE_WMS_BROKER_CFLAGS="-I$ac_glite_wms_broker_prefix/include"
        GLITE_WMS_BROKER_LIBS="$ac_glite_wms_broker_lib -lglite_wms_broker"
        ifelse([$2], , :, [$2])
    else
        GLITE_WMS_BROKER_CFLAGS=""
        GLITE_WMS_BROKER_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_BROKER_CFLAGS)
    AC_SUBST(GLITE_WMS_BROKER_LIBS)
])

