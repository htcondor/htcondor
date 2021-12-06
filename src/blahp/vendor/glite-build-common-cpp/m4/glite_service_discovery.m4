dnl Usage:
dnl AC_GLITE_SERVICE_DISCOVERY
dnl - GLITE_SERVICE_DISCOVERY_API_C_LIBS
dnl - GLITE_SERVICE_DISCOVERY_API_C_STATIC_LIBS

AC_DEFUN([AC_GLITE_SERVICE_DISCOVERY],
[
    ac_glite_sd_prefix=$GLITE_LOCATION

    have_sd_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_SERVICE_DISCOVERY_API_C, service-discovery-api-c, have_sd_pkgconfig=yes, have_sd_pkgconfig=no)
    if test "x$have_sd_pkgconfig" = "xyes" ; then
        GLITE_SERVICE_DISCOVERY_API_C_STATIC_LIBS=""
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_sd_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_sd_lib="-L$ac_glite_sd_prefix/lib64"
        else
            ac_glite_sd_lib="-L$ac_glite_sd_prefix/lib"
        fi

	GLITE_SERVICE_DISCOVERY_API_C_LIBS="$ac_glite_sd_lib -lglite-sd-c"
	GLITE_SERVICE_DISCOVERY_API_C_STATIC_LIBS="$ac_glite_sd_prefix/lib/libglite-sd-c.a"
	GLITE_SERVICE_DISCOVERY_API_C_CFLAGS="-I$ac_glite_sd_prefix/include"
	ifelse([$2], , :, [$2])
    else
	GLITE_SERVICE_DISCOVERY_API_C_LIBS=""
	GLITE_SERVICE_DISCOVERY_API_C_STATIC_LIBS=""
        GLITE_SERVICE_DISCOVERY_API_C_CFLAGS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_SERVICE_DISCOVERY_API_C_LIBS)
    AC_SUBST(GLITE_SERVICE_DISCOVERY_API_C_STATIC_LIBS)
    AC_SUBST(GLITE_SERVICE_DISCOVERY_API_C_CFLAGS)
])

