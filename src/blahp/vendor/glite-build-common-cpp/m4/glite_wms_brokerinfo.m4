dnl Usage:
dnl AC_GLITE_WMS_BROKERINFO
dnl - GLITE_WMS_BROKERINFO_LIBS

AC_DEFUN([AC_GLITE_WMS_BROKERINFO],
[
    ac_glite_wms_brokerinfo_prefix=$GLITE_LOCATION

    have_binfo_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_BROKERINFO, wms-brokerinfo, have_binfo_pkgconfig=yes, have_binfo_pkgconfig=no)

    if test "x$have_binfo_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Brokerinfo])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_brokerinfo_prefix" ; then
	if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_brokerinfo_lib="-L$ac_glite_wms_brokerinfo_prefix/lib64"
        else
            ac_glite_wms_brokerinfo_lib="-L$ac_glite_wms_brokerinfo_prefix/lib"
        fi

        GLITE_WMS_BROKERINFO_CFLAGS="-I$ac_glite_wms_bokerinfo_prefix/include"
	GLITE_WMS_BROKERINFO_LIBS="$ac_glite_wms_brokerinfo_lib -lglite_wms_brokerinfo"
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_BROKERINFO_CFLAGS=""
	GLITE_WMS_BROKERINFO_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_BROKERINFO_CFLAGS)
    AC_SUBST(GLITE_WMS_BROKERINFO_LIBS)
])

