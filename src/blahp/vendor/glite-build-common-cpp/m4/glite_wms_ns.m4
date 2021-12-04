dnl Usage:
dnl AC_GLITE_WMS_NS_COMMON
dnl - GLITE_WMS_NS_COMMON_CLIENT_COMMANDS_LIBS
dnl - GLITE_WMS_NS_COMMON_SERVER_COMMANDS_LIBS
dnl - GLITE_WMS_NS_COMMON_COMMON_COMMANDS_LIBS
dnl - GLITE_WMS_NS_COMMON_GLITE_PIPE_LIBS
dnl AC_GLITE_WMS_NS_CLIENT
dnl - GLITE_WMS_NS_CLIENT_LIBS
dnl AC_GLITE_WMS_COMMON_QUOTA
dnl - GLITE_WMS_COMMON_QUOTA_LIBS

AC_DEFUN([AC_GLITE_WMS_NS_COMMON],
[
    ac_glite_wms_ns_common_prefix=$GLITE_LOCATION

    if test -n "ac_glite_wms_ns_common" ; then
	dnl
	dnl 
	dnl
        ac_glite_wms_ns_common_lib="-L$ac_glite_wms_ns_common_prefix/lib"
	GLITE_WMS_NS_COMMON_CLIENT_COMMANDS_LIBS="$ac_glite_wms_ns_common_lib -lnsclientcommands"
	GLITE_WMS_NS_COMMON_SERVER_COMMANDS_LIBS="$ac_glite_wms_ns_common_lib -lnsservercommands"
	GLITE_WMS_NS_COMMON_COMMON_COMMANDS_LIBS="$ac_glite_wms_ns_common_lib -lnscommoncommands"
	GLITE_WMS_NS_COMMON_GLITE_PIPE_LIBS="$ac_glite_wms_ns_common_lib -l_glite_pipe"
	
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_NS_COMMON_CLIENT_COMMANDS_LIBS=""
        GLITE_WMS_NS_COMMON_SERVER_COMMANDS_LIBS=""
	GLITE_WMS_NS_COMMON_COMMON_COMMANDS_LIBS=""
        GLITE_WMS_NS_COMMON_GLITE_PIPE_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_NS_COMMON_CLIENT_COMMANDS_LIBS)
    AC_SUBST(GLITE_WMS_NS_COMMON_SERVER_COMMANDS_LIBS)
    AC_SUBST(GLITE_WMS_NS_COMMON_COMMON_COMMANDS_LIBS)
    AC_SUBST(GLITE_WMS_NS_COMMON_GLITE_PIPE_LIBS)
])

AC_DEFUN([AC_GLITE_WMS_NS_CLIENT],
[
    ac_glite_wms_ns_client_prefix=$GLITE_LOCATION

    if test -n "ac_glite_wms_ns_client_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_wms_ns_client_lib="-L$ac_glite_wms_ns_client_prefix/lib"
        GLITE_WMS_NS_CLIENT_LIBS="$ac_glite_wms_ns_lib -lglite_wms_ns_client"
        ifelse([$2], , :, [$2])
    else
        GLITE_WMS_NS_CLIENT_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_NS_CLIENT_LIBS)
])

AC_DEFUN([AC_GLITE_WMS_COMMON_QUOTA],
[
    ac_glite_wms_common_quota_prefix=$GLITE_LOCATION

    if test -n "ac_glite_wms_common_quota_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_wms_common_quota_lib="-L$ac_glite_wms_common_quota_prefix/lib"
        GLITE_WMS_COMMON_QUOTA_LIBS="$ac_glite_wms_common_quota_lib -lglite_wms_quota"
        ifelse([$2], , :, [$2])
    else
        GLITE_WMS_COMMON_QUOTA_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_COMMON_QUOTA_LIBS)
])
