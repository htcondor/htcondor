dnl Usage:
dnl AC_GLITE_WMS_HELPER
dnl - GLITE_WMS_HELPER_LIBS
dnl - GLITE_WMS_HELPER_JOBADAPTER_LIBS
dnl - GLITE_WMS_HELPER_BROKER_ISM_LIBS
AC_DEFUN([AC_GLITE_WMS_HELPER],
[
    ac_glite_wms_helper_prefix=$GLITE_LOCATION

    have_helper_pkgconfig=yes
    PKG_CHECK_MODULES(GLITE_WMS_HELPER, wms-helper, , have_helper_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_HELPER_JOBADAPTER, wms-helper-jobadapter, , have_helper_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_HELPER_BROKER_ISM, wms-helper-broker-ism, , have_helper_pkgconfig=no)

    if test "x$have_helper_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Helper])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_helper_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_helper_lib="-L$ac_glite_wms_helper_prefix/lib64"
        else
            ac_glite_wms_helper_lib="-L$ac_glite_wms_helper_prefix/lib"
        fi

        GLITE_WMS_HELPER_CFLAGS="-I$ac_glite_wms_helper_prefix/include"
	GLITE_WMS_HELPER_LIBS="$ac_glite_wms_helper_lib -lglite_wms_helper"
        GLITE_WMS_HELPER_JOBADAPTER_CFLAGS="-I$ac_glite_wms_helper_prefix/include"
        GLITE_WMS_HELPER_JOBADAPTER_LIBS="$ac_glite_wms_helper_lib -lglite_wms_helper_jobadapter"
        GLITE_WMS_HELPER_BROKER_ISM_CFLAGS="-I$ac_glite_wms_helper_prefix/include"
	GLITE_WMS_HELPER_BROKER_ISM_LIBS="$ac_glite_wms_helper_ism_lib -lglite_wms_helper_broker_ism"
	ifelse([$2], , :, [$2])
    else
        GLITE_WMS_HELPER_CFLAGS=""
	GLITE_WMS_HELPER_LIBS=""
        GLITE_WMS_HELPER_JOBADAPTER_CFLAGS=""
	GLITE_WMS_HELPER_JOBADAPTER_LIBS=""
        GLITE_WMS_HELPER_BROKER_ISM_CFLAGS=""
	GLITE_WMS_HELPER_BROKER_ISM_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_HELPER_CFLAGS)
    AC_SUBST(GLITE_WMS_HELPER_LIBS)
    AC_SUBST(GLITE_WMS_HELPER_JOBADAPTER_CFLAGS)
    AC_SUBST(GLITE_WMS_HELPER_JOBADAPTER_LIBS)
    AC_SUBST(GLITE_WMS_HELPER_BROKER_ISM_CFLAGS)
    AC_SUBST(GLITE_WMS_HELPER_BROKER_ISM_LIBS)

])

