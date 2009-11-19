dnl Usage:
dnl AC_GLOBUS(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - GLOBUS_LOCATION
dnl - GLOBUS_NOTHR_FLAVOR
dnl - GLOBUS_NOTHR_CFLAGS
dnl - GLOBUS_NOTHR_LIBS
dnl - GLOBUS_COMMON_NOTHR_LIBS
dnl - GLOBUS_STATIC_COMMON_NOTHR_LIBS
dnl - GLOBUS_FTP_CLIENT_NOTHR_LIBS
dnl - GLOBUS_SSL_NOTHR_LIBS
dnl - GLOBUS_STATIC_SSL_NOTHR_LIBS
dnl - GLOBUS_GSS_NOTHR_LIBS

AC_DEFUN(AC_GLOBUS,
[
    AC_ARG_WITH(openssl_prefix,
	[  --with-openssl-prefix=PFX     prefix where OpenSSL is installed. (/)],
	[],
        with_openssl_prefix=/)

    AC_ARG_WITH(globus_prefix,
	[  --with-globus-prefix=PFX     prefix where GLOBUS is installed. (/opt/globus)],
	[],
        with_globus_prefix=${GLOBUS_LOCATION:-/opt/globus})

    AC_ARG_WITH(globus_nothr_flavor,
	[  --with-globus-nothr-flavor=flavor [default=gcc32dbg]],
	[],
        with_globus_nothr_flavor=${GLOBUS_FLAVOR:-gcc32dbg})

    AC_MSG_RESULT(["GLOBUS nothread flavor is $with_globus_nothr_flavor"])

    ac_cv_globus_nothr_valid=no

    GLOBUS_NOTHR_CFLAGS="$with_openssl_prefix/include -I$with_globus_prefix/include/$with_globus_nothr_flavor"

    ac_globus_ldlib="-L$with_globus_prefix/lib"

    GLOBUS_COMMON_NOTHR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_nothr_flavor"

    GLOBUS_STATIC_COMMON_NOTHR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_nothr_flavor.a"

    GLOBUS_FTP_CLIENT_NOTHR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_nothr_flavor"

    GLOBUS_GSS_NOTHR_LIBS="$with_globus_prefix/lib/libglobus_gss_assist_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_gssapi_gsi_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_gsi_callback_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_oldgaa_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_gsi_proxy_core_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_gsi_credential_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_gsi_cert_utils_$with_globus_nothr_flavor.a $with_openssl_prefix/lib/libssl.a $with_globus_prefix/lib/libglobus_gsi_sysconfig_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_openssl_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_proxy_ssl_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_openssl_error_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_callout_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libglobus_common_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libltdl_$with_globus_nothr_flavor.a"

    GLOBUS_SSL_NOTHR_LIBS="$with_openssl_prefix/lib/libssl.a $with_openssl_prefix/lib/libcrypto.a"

    GLOBUS_STATIC_SSL_NOTHR_LIBS="$with_openssl_prefix/lib/libssl.a $with_openssl_prefix/lib/libcrypto.a"

    dnl Needed by LCAS/LCMAPS voms plugins
    GLOBUS_GSI_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_nothr_flavor"

    dnl
    dnl check nothr openssl header
    dnl
    ac_globus_nothr_ssl="$with_openssl_prefix/include/openssl"

    AC_MSG_CHECKING([for $ac_globus_nothr_ssl/ssl.h])

    if test ! -f "$ac_globus_nothr_ssl/ssl.h" ; then
	ac_globus_nothr_ssl=""
	AC_MSG_RESULT([no])
    else
	AC_MSG_RESULT([yes])
    fi

    AC_MSG_CHECKING([for openssl nothr])

    if test -n "$ac_globus_nothr_ssl" ; then
	GLOBUS_NOTHR_CFLAGS="-I$ac_globus_nothr_ssl -I$GLOBUS_NOTHR_CFLAGS"
    fi

    if test -n "$ac_globus_nothr_ssl" ; then
        dnl
        dnl maybe do some complex test of globus instalation here later
        dnl
        ac_save_CFLAGS=$CFLAGS
        CFLAGS="$GLOBUS_NOTHR_CFLAGS $CFLAGS"
        AC_TRY_COMPILE([
             #include "ssl.h"
             #include "globus_gss_assist.h"
           ],
           [globus_gss_assist_ex aex],
           [ac_cv_globus_nothr_valid=yes],
           [ac_cv_globus_nothr_valid=no])
        CFLAGS=$ac_save_CFLAGS
        AC_MSG_RESULT([$ac_cv_globus_nothr_valid])
    fi

    if test x$ac_cv_globus_nothr_valid = xyes ; then
	GLOBUS_LOCATION=$with_globus_prefix
	GLOBUS_NOTHR_FLAVOR=$with_globus_nothr_flavor
	ifelse([$2], , :, [$2])
    else
	GLOBUS_NOTHR_CFLAGS=""
	GLOBUS_NOTHR_LIBS=""
	GLOBUS_COMMON_NOTHR_LIBS=""
	GLOBUS_STATIC_COMMON_NOTHR_LIBS=""
        GLOBUS_FTP_CLIENT_NOTHR_LIBS=""
	GLOBUS_SSL_NOTHR_LIBS=""
	GLOBUS_STATIC_SSL_NOTHR_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLOBUS_LOCATION)
    AC_SUBST(GLOBUS_NOTHR_FLAVOR)
    AC_SUBST(GLOBUS_NOTHR_CFLAGS)
    AC_SUBST(GLOBUS_NOTHR_LIBS)
    AC_SUBST(GLOBUS_COMMON_NOTHR_LIBS)
    AC_SUBST(GLOBUS_STATIC_COMMON_NOTHR_LIBS)
    AC_SUBST(GLOBUS_FTP_CLIENT_NOTHR_LIBS)
    AC_SUBST(GLOBUS_SSL_NOTHR_LIBS)
    AC_SUBST(GLOBUS_STATIC_SSL_NOTHR_LIBS)
    AC_SUBST(GLOBUS_GSS_NOTHR_LIBS)
])

