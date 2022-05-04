dnl Usage:
dnl AC_GLITE_WMS_COMMON
dnl - GLITE_WMS_COMMON_CONF_LIBS
dnl - GLITE_WMS_COMMON_CONF_WRAPPER_LIBS
dnl - GLITE_WMS_COMMON_CONFIG_LIBS
dnl - GLITE_WMS_COMMON_LDIF2CLASSADS_LIBS
dnl - GLITE_WMS_COMMON_LOGGER_LIBS
dnl - GLITE_WMS_COMMON_PROCESS_LIBS
dnl - GLITE_WMS_COMMON_UT_UTIL_LIBS
dnl - GLITE_WMS_COMMON_UT_FTP_LIBS
dnl - GLITE_WMS_COMMON_UT_II_LIBS
dnl - GLITE_WMS_COMMON_QUOTA_LIBS

AC_DEFUN([AC_GLITE_WMS_COMMON],
[
    ac_glite_wms_common_prefix=$GLITE_LOCATION

    have_common_pkgconfig=yes
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_CONF, wms-common-conf, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_LDIF2CLASSADS, wms-common-ldif2classads, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_LOGGER, wms-common-logger, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_PROCESS, wms-common-process, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_UT_UTIL, wms-common-util, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_UT_II, wms-common-ii, , have_common_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_COMMON_QUOTA, wms-common-quota, , have_common_pkgconfig=no)

    if test "x$have_common_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Common])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_common_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_common_lib="-L$ac_glite_wms_common_prefix/lib64"
        else
            ac_glite_wms_common_lib="-L$ac_glite_wms_common_prefix/lib"
        fi

        GLITE_WMS_COMMON_CONF_LIBS="$ac_glite_wms_common_lib -lglite_wms_conf"
        GLITE_WMS_COMMON_LDIF2CLASSADS_LIBS="$ac_glite_wms_common_lib -lglite_wms_LDIF2ClassAd"
        GLITE_WMS_COMMON_LOGGER_LIBS="$ac_glite_wms_common_lib -lglite_wms_logger"
        GLITE_WMS_COMMON_PROCESS_LIBS="$ac_glite_wms_common_lib -lglite_wms_process"
        GLITE_WMS_COMMON_UT_UTIL_LIBS="$ac_glite_wms_common_lib -lglite_wms_util"
        GLITE_WMS_COMMON_UT_II_LIBS="$ac_glite_wms_common_lib -lglite_wms_iiattrutil"
        GLITE_WMS_COMMON_QUOTA_LIBS="$ac_glite_wms_common_lib -lglite_wms_quota"
dnl        GLITE_WMS_COMMON_CONF_WRAPPER_LIBS="$ac_glite_wms_common_lib -lglite_wms_conf_wrapper"
dnl        GLITE_WMS_COMMON_CONFIG_LIBS="$GLITE_WMS_COMMON_CONF_LIBS -lglite_wms_conf_wrapper"
dnl        GLITE_WMS_COMMON_UT_FTP_LIBS="$ac_glite_wms_common_lib -lglite_wms_globus_ftp_util" 
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_COMMON_CONF_LIBS=""
	GLITE_WMS_COMMON_CONF_CFLAGS=""
	GLITE_WMS_COMMON_LDIF2CLASSADS_LIBS=""
	GLITE_WMS_COMMON_LDIF2CLASSADS_CFLAGS=""
	GLITE_WMS_COMMON_LOGGER_LIBS=""
	GLITE_WMS_COMMON_LOGGER_CFLAGS=""
	GLITE_WMS_COMMON_PROCESS_LIBS=""
	GLITE_WMS_COMMON_PROCESS_CFLAGS=""
	GLITE_WMS_COMMON_UT_UTIL_LIBS=""
	GLITE_WMS_COMMON_UT_UTIL_CFLAGS=""
	GLITE_WMS_COMMON_UT_II_LIBS=""
        GLITE_WMS_COMMON_UT_II_CFLAGS=""
        GLITE_WMS_COMMON_QUOTA_LIBS=""
        GLITE_WMS_COMMON_QUOTA_CFLAGS=""
dnl     GLITE_WMS_COMMON_CONF_WRAPPER_LIBS=""
dnl     GLITE_WMS_COMMON_CONFIG_LIBS=""
dnl     GLITE_WMS_COMMON_UT_FTP_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_COMMON_CONF_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_CONF_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_LDIF2CLASSADS_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_LDIF2CLASSADS_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_LOGGER_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_LOGGER_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_PROCESS_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_PROCESS_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_UT_UTIL_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_UT_UTIL_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_UT_II_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_UT_II_CFLAGS)
    AC_SUBST(GLITE_WMS_COMMON_QUOTA_LIBS)
    AC_SUBST(GLITE_WMS_COMMON_QUOTA_CFLAGS)
dnl    AC_SUBST(GLITE_WMS_COMMON_CONF_WRAPPER_LIBS)
dnl    AC_SUBST(GLITE_WMS_COMMON_CONFIG_LIBS)
dnl    AC_SUBST(GLITE_WMS_COMMON_UT_FTP_LIBS)

])

