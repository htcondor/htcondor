dnl Usage:
dnl AC_GLOBUS(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - GLOBUS_LOCATION
dnl - GLOBUS_NOTHR_FLAVOR
dnl - GLOBUS_THR_FLAVOR
dnl - GLOBUS_NOTHR_CFLAGS
dnl - GLOBUS_THR_CFLAGS
dnl - GLOBUS_NOTHR_LIBS
dnl - GLOBUS_THR_LIBS
dnl - GLOBUS_COMMON_NOTHR_LIBS
dnl - GLOBUS_COMMON_THR_LIBS
dnl - GLOBUS_STATIC_COMMON_NOTHR_LIBS
dnl - GLOBUS_STATIC_COMMON_THR_LIBS
dnl - GLOBUS_FTP_CLIENT_NOTHR_LIBS
dnl - GLOBUS_FTP_CLIENT_THR_LIBS
dnl - GLOBUS_SSL_NOTHR_LIBS
dnl - GLOBUS_SSL_THR_LIBS
dnl - GLOBUS_STATIC_SSL_NOTHR_LIBS
dnl - GLOBUS_STATIC_SSL_THR_LIBS
dnl - GLOBUS_GSS_NOTHR_LIBS
dnl - GLOBUS_GSS_THR_LIBS
dnl - GLOBUS_LDAP_THR_LIBS

AC_DEFUN(AC_GLOBUS,
[
    AC_ARG_WITH(openssl_prefix,
	[  --with-openssl-prefix=PFX     prefix where OpenSSL is installed. (/)],
	[],
	with_openssl_prefix=/usr)

    AC_ARG_WITH(globus_prefix,
	[  --with-globus-prefix=PFX     prefix where GLOBUS is installed. (/opt/globus)],
	[],
        with_globus_prefix=${GLOBUS_LOCATION:-/opt/globus})

    AC_ARG_WITH(voms_prefix,
	[  --with-voms-prefix=PFX     prefix where VOMS is installed. (/opt/glite)],
	[],
	with_voms_prefix=/opt/glite)

    AC_ARG_WITH(globus_nothr_flavor,
	[  --with-globus-nothr-flavor=flavor [default=gcc32dbg]],
	[],
        with_globus_nothr_flavor=${GLOBUS_FLAVOR:-gcc32dbg})

    AC_MSG_RESULT(["GLOBUS nothread flavor is $with_globus_nothr_flavor"])

    AC_ARG_WITH(globus_thr_flavor,
        [  --with-globus-thr-flavor=flavor [default=gcc32dbgpthr]],
        [],
        with_globus_thr_flavor=${GLOBUS_FLAVOR:-gcc32dbgpthr})

    AC_MSG_RESULT(["GLOBUS thread flavor is $with_globus_thr_flavor"])

    ac_cv_globus_nothr_valid=no
    ac_cv_globus_thr_valid1=no

    GLOBUS_NOTHR_CFLAGS="$with_openssl_prefix/include -I$with_globus_prefix/include/$with_globus_nothr_flavor -I$with_voms_prefix/include"
    GLOBUS_THR_CFLAGS="$with_openssl_prefix/include -I$with_globus_prefix/include/$with_globus_thr_flavor -I$with_voms_prefix/include"

    ac_globus_ldlib="-L$with_openssl_prefix/lib -L$with_globus_prefix/lib -L$with_voms_prefix/lib"

    GLOBUS_COMMON_NOTHR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_nothr_flavor"
    GLOBUS_COMMON_THR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_thr_flavor"

    GLOBUS_STATIC_COMMON_NOTHR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_nothr_flavor.a"
    GLOBUS_STATIC_COMMON_THR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_thr_flavor.a"

    GLOBUS_FTP_CLIENT_NOTHR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_nothr_flavor"
    GLOBUS_FTP_CLIENT_THR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_thr_flavor"

    GLOBUS_GSS_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_nothr_flavor -lglobus_gss_assist_$with_globus_nothr_flavor"
    GLOBUS_GSS_THR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_thr_flavor -lglobus_gss_assist_$with_globus_thr_flavor"

    GLOBUS_SSL_NOTHR_LIBS="$ac_globus_ldlib -lssl -lcrypto"
    GLOBUS_SSL_THR_LIBS="$ac_globus_ldlib -lssl -lcrypto"

    GLOBUS_STATIC_SSL_NOTHR_LIBS="$with_openssl_prefix/lib/libssl.a $with_openssl_prefix/lib/libcrypto.a"
    GLOBUS_STATIC_SSL_THR_LIBS="$with_openssl_prefix/lib/libssl.a $with_openssl_prefix/lib/libcrypto.a"

    dnl Needed by LCAS/LCMAPS voms plugins
    GLOBUS_GSI_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_nothr_flavor"
    GLOBUS_GSI_THR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_thr_flavor"

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
    else
	AC_MSG_RESULT([no])
    fi

    dnl
    dnl check thr openssl header
    dnl
    ac_globus_thr_ssl="$with_openssl_prefix/include/openssl"

    AC_MSG_CHECKING([for $ac_globus_thr_ssl/ssl.h])

    if test ! -f "$ac_globus_thr_ssl/ssl.h" ; then
        ac_globus_thr_ssl=""
        AC_MSG_RESULT([no])
    else
        AC_MSG_RESULT([yes])
    fi

    if test -n "$ac_globus_thr_ssl" ; then
        GLOBUS_THR_CFLAGS="-I$ac_globus_thr_ssl -I$GLOBUS_THR_CFLAGS"
    fi

    AC_MSG_CHECKING([checking openssl thr])

    if test -n "$ac_globus_thr_ssl" ; then
	dnl
	dnl maybe do some complex test of globus instalation here later
	dnl
	ac_save_CFLAGS=$CFLAGS
	CFLAGS="$GLOBUS_THR_CFLAGS $CFLAGS"
	AC_TRY_COMPILE([
	     #include "openssl/ssl.h"
	     #include "globus_gss_assist.h"
	   ],
           [globus_gss_assist_ex aex],
	   [ac_cv_globus_thr_valid1=yes],
           [ac_cv_globus_thr_valid1=no])
        CFLAGS=$ac_save_CFLAGS
        AC_MSG_RESULT([$ac_cv_globus_thr_valid1])
    else
	AC_MSG_RESULT([no])
    fi

    if test x$ac_cv_globus_nothr_valid = xyes -a x$ac_cv_globus_thr_valid1 = xyes ; then
	GLOBUS_LOCATION=$with_globus_prefix
	GLOBUS_NOTHR_FLAVOR=$with_globus_nothr_flavor
        GLOBUS_THR_FLAVOR=$with_globus_thr_flavor
	ifelse([$2], , :, [$2])
    else
	GLOBUS_NOTHR_CFLAGS=""
	GLOBUS_THR_CFLAGS=""
	GLOBUS_NOTHR_LIBS=""
	GLOBUS_THR_LIBS=""
	GLOBUS_COMMON_NOTHR_LIBS=""
	GLOBUS_COMMON_THR_LIBS=""
	GLOBUS_STATIC_COMMON_NOTHR_LIBS=""
        GLOBUS_STATIC_COMMON_THR_LIBS=""
        GLOBUS_FTP_CLIENT_NOTHR_LIBS=""
	GLOBUS_FTP_CLIENT_THR_LIBS=""
	GLOBUS_SSL_NOTHR_LIBS=""
	GLOBUS_SSL_THR_LIBS=""
	GLOBUS_STATIC_SSL_NOTHR_LIBS=""
        GLOBUS_STATIC_SSL_THR_LIBS=""
	GLOBUS_LDAP_THR_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLOBUS_LOCATION)
    AC_SUBST(GLOBUS_NOTHR_FLAVOR)
    AC_SUBST(GLOBUS_THR_FLAVOR)
    AC_SUBST(GLOBUS_NOTHR_CFLAGS)
    AC_SUBST(GLOBUS_THR_CFLAGS)
    AC_SUBST(GLOBUS_NOTHR_LIBS)
    AC_SUBST(GLOBUS_THR_LIBS)
    AC_SUBST(GLOBUS_COMMON_NOTHR_LIBS)
    AC_SUBST(GLOBUS_COMMON_THR_LIBS)
    AC_SUBST(GLOBUS_STATIC_COMMON_NOTHR_LIBS)
    AC_SUBST(GLOBUS_STATIC_COMMON_THR_LIBS)
    AC_SUBST(GLOBUS_FTP_CLIENT_NOTHR_LIBS)
    AC_SUBST(GLOBUS_FTP_CLIENT_THR_LIBS)
    AC_SUBST(GLOBUS_SSL_NOTHR_LIBS)
    AC_SUBST(GLOBUS_SSL_THR_LIBS)
    AC_SUBST(GLOBUS_STATIC_SSL_NOTHR_LIBS)
    AC_SUBST(GLOBUS_STATIC_SSL_THR_LIBS)
    AC_SUBST(GLOBUS_GSS_NOTHR_LIBS)
    AC_SUBST(GLOBUS_GSS_THR_LIBS)
    AC_SUBST(GLOBUS_LDAP_THR_LIBS)
])

