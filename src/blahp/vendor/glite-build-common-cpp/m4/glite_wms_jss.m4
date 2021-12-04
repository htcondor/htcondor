dnl Usage:
dnl AC_GLITE_WMS_JSS
dnl - GLITE_WMS_JSS_COMMON_LIBS
dnl - GLITE_WMS_JSS_CONTROLLER_LIBS
dnl - GLITE_WMS_JSS_CONTROLLER_ADAPTER_LIBS
dnl - GLITE_WMS_JSS_CONTROLLER_WRAPPER_LIBS
dnl - GLITE_WMS_JSS_LOGMONITOR_LIBS

AC_DEFUN([AC_GLITE_WMS_JSS],
[
    ac_glite_wms_jss_prefix=$GLITE_LOCATION

    have_jss_pkgconfig=yes
    PKG_CHECK_MODULES(GLITE_WMS_JSS_COMMON, wms-jss-common, , have_jss_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_JSS_CONTROLLER, wms-jss-controller, , have_jss_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_JSS_CONTROLLER_ADAPTER, wms-jss-controller-adapter, , have_jss_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_JSS_CONTROLLER_WRAPPER, wms-jss-controller-wrapper, , have_jss_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_JSS_LOGMONITOR, wms-jss-logmonitor, , have_jss_pkgconfig=no)

    if test "x$have_jss_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS JSS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_jss_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_jss_lib="-L$ac_glite_wms_jss_prefix/lib64"
        else 
            ac_glite_wms_jss_lib="-L$ac_glite_wms_jss_prefix/lib"
        fi

        GLITE_WMS_JSS_COMMON_CFLAGS="-I$ac_glite_wms_jss_prefix/include"
        GLITE_WMS_JSS_COMMON_LIBS="$ac_glite_wms_jss_lib -lglite_wms_jss_common"
        GLITE_WMS_JSS_CONTROLLER_CFLAGS="-I$ac_glite_wms_jss_prefix/include"
	GLITE_WMS_JSS_CONTROLLER_LIBS="$ac_glite_wms_jss_lib -lglite_wms_jss_controller"
        GLITE_WMS_JSS_CONTROLLER_ADAPTER_CFLAGS="-I$ac_glite_wms_jss_prefix/include"
	GLITE_WMS_JSS_CONTROLLER_ADAPTER_LIBS="$ac_glite_wms_jss_lib -lglite_wms_jss_controller_adapter"
        GLITE_WMS_JSS_CONTROLLER_WRAPPER_CFLAGS="-I$ac_glite_wms_jss_prefix/include"
	GLITE_WMS_JSS_CONTROLLER_WRAPPER_LIBS="$ac_glite_wms_jss_lib -lglite_wms_jss_controller_wrapper"
        GLITE_WMS_JSS_LOGMONITOR_CFLAGS="-I$ac_glite_wms_jss_prefix/include"
	GLITE_WMS_JSS_LOGMONITOR_LIBS="$ac_glite_wms_jss_lib -lglite_wms_jss_logmonitor"
	ifelse([$2], , :, [$2])
    else
        GLITE_WMS_JSS_COMMON_CFLAGS=""
	GLITE_WMS_JSS_COMMON_LIBS=""
        GLITE_WMS_JSS_CONTROLLER_CFLAGS=""
	GLITE_WMS_JSS_CONTROLLER_LIBS=""
        GLITE_WMS_JSS_CONTROLLER_ADAPTER_CFLAGS=""
	GLITE_WMS_JSS_CONTROLLER_ADAPTER_LIBS=""
        GLITE_WMS_JSS_CONTROLLER_WRAPPER_CFLAGS=""
	GLITE_WMS_JSS_CONTROLLER_WRAPPER_LIBS=""
        GLITE_WMS_JSS_LOGMONITOR_CFLAGS=""
	GLITE_WMS_JSS_LOGMONITOR_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_JSS_COMMON_CFLAGS)
    AC_SUBST(GLITE_WMS_JSS_COMMON_LIBS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_CFLAGS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_LIBS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_ADAPTER_CFLAGS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_ADAPTER_LIBS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_WRAPPER_CFLAGS)
    AC_SUBST(GLITE_WMS_JSS_CONTROLLER_WRAPPER_LIBS)
    AC_SUBST(GLITE_WMS_JSS_LOGMONITOR_CFLAGS)
    AC_SUBST(GLITE_WMS_JSS_LOGMONITOR_LIBS)

])

