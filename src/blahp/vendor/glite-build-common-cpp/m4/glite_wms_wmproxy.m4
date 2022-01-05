dnl Usage:
dnl AC_GLITE_WMS_WMPROXY
dnl - GLITE_WMS_WMPROXY_CFLAGS
dnl - GLITE_WMS_WMPROXY_LIBS

AC_DEFUN([AC_GLITE_WMS_WMPROXY],
[
    ac_glite_wms_wmproxy_prefix=$GLITE_LOCATION

    have_wmproxy_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_WMPROXY, wmproxy-api-cpp, have_wmproxy_pkgconfig=yes, have_wmproxy_pkgconfig=no)

    if test "x$have_wmproxy_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMProxy API C++])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_wmproxy_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_wmproxy_lib="-L$ac_glite_wms_wmproxy_prefix/lib64"
        else
            ac_glite_wms_wmproxy_lib="-L$ac_glite_wms_wmproxy_prefix/lib"
        fi

        GLITE_WMS_WMPROXY_CFLAGS="-I$ac_glite_wms_wmproxy_prefix/include"
	GLITE_WMS_WMPROXY_LIBS="$ac_glite_wms_wmproxy_lib -lglite_wms_wmproxy_api_cpp"
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_WMPROXY_CFLAGS=""
	GLITE_WMS_WMPROXY_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_WMPROXY_CFLAGS)
    AC_SUBST(GLITE_WMS_WMPROXY_LIBS)
])

AC_DEFUN([AC_GLITE_WMS_WSDL],
[
    AC_ARG_WITH(wms_wsdl_location,
        [  --with-wms-wsdl-location=PATH    path of the wsdl file set],
        [],
        with_wms_wsdl_location=${GLITE_LOCATION:-/usr})

    GLITE_WMS_WSDL_PATH=${with_wms_wsdl_location}/share/wsdl/wms
    AC_MSG_CHECKING([WMS WSDL in ${with_wms_wsdl_location}/share/wsdl/wms])

    GLITE_WMS_WSDL=${GLITE_WMS_WSDL_PATH}/WMProxy.wsdl
    GLITE_WMS_DLI_WSDL=${GLITE_WMS_WSDL_PATH}/DataLocationInterface.wsdl
    if test -f "$GLITE_WMS_WSDL" -a -f "$GLITE_WMS_DLI_WSDL"; then
        AC_SUBST(GLITE_WMS_WSDL_PATH)
        AC_SUBST(GLITE_WMS_WSDL)
        AC_SUBST(GLITE_WMS_DLI_WSDL)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([no])])
    fi
])

