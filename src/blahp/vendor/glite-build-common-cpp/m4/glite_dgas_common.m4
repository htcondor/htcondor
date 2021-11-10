dnl Usage:
dnl AC_GLITE_DGAS_COMMON
dnl - GLITE_DGAS_COMMON_DBHELPER_LIBS
dnl - GLITE_DGAS_COMMON_CONFIG_LIBS
dnl - GLITE_DGAS_COMMON_DIGESTS_LIBS
dnl - GLITE_DGAS_COMMON_LOG_LIBS
dnl - GLITE_DGAS_COMMON_GRIDMAPFILE_LIBS
dnl - GLITE_DGAS_COMMON_LOCK_LIBS
dnl - GLITE_DGAS_COMMON_XMLUTIL_LIBS
dnl - GLITE_DGAS_COMMON_LDAP_LIBS
dnl - GLITE_DGAS_COMMON_TLS_LIBS

AC_DEFUN([AC_GLITE_DGAS_COMMON],
[
    ac_glite_dgas_common_prefix=$GLITE_LOCATION
    if test "x$host_cpu" = "xx86_64"; then
       	ac_glite_dgas_common_lib="-L$ac_glite_dgas_common_prefix/lib64"
    else
    	ac_glite_dgas_common_lib="-L$ac_glite_dgas_common_prefix/lib" 
    fi

    if test -h "/usr/lib64" ; then
    	ac_glite_dgas_common_lib="-L$ac_glite_dgas_common_prefix/lib" 
    fi

    if ! test -e "/usr/lib64" ; then
    	ac_glite_dgas_common_lib="-L$ac_glite_dgas_common_prefix/lib" 
    fi

    if test -n "ac_glite_dgas_common_prefix" ; then
	dnl
	dnl
	dnl
        GLITE_DGAS_COMMON_DBHELPER_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_dbhelper"
	GLITE_DGAS_COMMON_CONFIG_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_config"
	GLITE_DGAS_COMMON_DIGESTS_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_digests"
	GLITE_DGAS_COMMON_LOG_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_log"
	GLITE_DGAS_COMMON_GRIDMAPFILE_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_gridMapFile"
	GLITE_DGAS_COMMON_LOCK_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_lock"
	GLITE_DGAS_COMMON_XMLUTIL_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_xmlutil"
	GLITE_DGAS_COMMON_LDAP_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_ldap"
	GLITE_DGAS_COMMON_TLS_LIBS="$ac_glite_dgas_common_lib -lglite_dgas_tls_gsisocket_pp -lglite_dgas_tls_exception"
	ifelse([$2], , :, [$2])
    else
	GLITE_DGAS_COMMON_DBHELPER_LIBS=""
        GLITE_DGAS_COMMON_CONFIG_LIBS=""
        GLITE_DGAS_COMMON_DIGESTS_LIBS=""
        GLITE_DGAS_COMMON_LOG_LIBS=""
        GLITE_DGAS_COMMON_GRIDMAPFILE_LIBS=""
        GLITE_DGAS_COMMON_LOCK_LIBS=""
        GLITE_DGAS_COMMON_XMLUTIL_LIBS=""
        GLITE_DGAS_COMMON_LDAP_LIBS=""
        GLITE_DGAS_COMMON_TLS_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_DGAS_COMMON_DBHELPER_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_CONFIG_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_DIGESTS_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_LOG_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_GRIDMAPFILE_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_LOCK_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_XMLUTIL_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_LDAP_LIBS)
    AC_SUBST(GLITE_DGAS_COMMON_TLS_LIBS)
])
