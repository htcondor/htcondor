dnl
dnl Define CGSI-GSOAP
dnl
AC_DEFUN([AC_CGSI_GSOAP],
[
	AC_ARG_WITH(cgsi-gsoap-location,
	[  --with-cgsi-gsoap-location=PFX     prefix where CGSI GSOAP plugin is installed. (/opt/cgsi-gsoap)],
	[],
        with_cgsi_gsoap_location=${CGSI_GSOAP_LOCATION:-/opt/cgsi-gsoap})
     
	AC_MSG_RESULT([checking for cgsi-gsoap... ])

	if test -n "with_cgsi_gsoap_location" ; then
		CGSI_GSOAP_LOCATION="$with_cgsi_gsoap_location"
		CGSI_GSOAP_CFLAGS="-I$with_cgsi_gsoap_location/include"
        if test "x$host_cpu" = "xx86_64"; then
		    ac_cgsi_gsoap_ldlib="-L$with_cgsi_gsoap_location/lib64"
        else
    		ac_cgsi_gsoap_ldlib="-L$with_cgsi_gsoap_location/lib"
        fi
        if test -h "/usr/lib64" ; then
		    ac_cgsi_gsoap_ldlib="-L$with_cgsi_gsoap_location/lib"
        fi
        if ! test -e "/usr/lib64" ; then
		    ac_cgsi_gsoap_ldlib="-L$with_cgsi_gsoap_location/lib"
        fi
		ac_cgsi_gsop_version="gsoap_$GSOAP_MAIN_VERSION"
		CGSI_GSOAP_LIBS="$ac_cgsi_gsoap_ldlib -lcgsi_plugin_$ac_cgsi_gsop_version"
		CGSI_GSOAP_STATIC_LIBS="$with_cgsi_gsoap_location/lib/libcgsi_plugin_$ac_cgsi_gsop_version.a"
    else
		CGSI_GSOAP_LOCATION=""
		CGSI_GSOAP_CFLAGS=""
		CGSI_GSOAP_LIBS=""
		CGSI_GSOAP_STATIC_LIBS=""
    fi
       
    AC_MSG_RESULT([CGSI_GSOAP_LOCATION set to $CGSI_GSOAP_LOCATION])
    AC_MSG_RESULT([CGSI_GSOAP_CFLAGS set to $CGSI_GSOAP_CFLAGS])
    AC_MSG_RESULT([CGSI_GSOAP_LIBS set to $CGSI_GSOAP_LIBS])
    AC_MSG_RESULT([CGSI_GSOAP_STATIC_LIBS set to $CGSI_GSOAP_STATIC_LIBS])
    
    AC_SUBST(CGSI_GSOAP_LOCATION)
    AC_SUBST(CGSI_GSOAP_CFLAGS)
    AC_SUBST(CGSI_GSOAP_LIBS)
    AC_SUBST(CGSI_GSOAP_STATIC_LIBS)
])
