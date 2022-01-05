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

AC_DEFUN([AC_GLOBUS],
[
    AC_ARG_WITH(globus_prefix,
	[  --with-globus-prefix=PFX     prefix where GLOBUS is installed. ($GLOBUS_LOCATION or /opt/globus or pkg-config)],
	[],
        with_globus_prefix=${GLOBUS_LOCATION:-/opt/globus})

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
    ac_cv_globus_thr_valid2=no

    GLOBUS_NOTHR_CFLAGS="$with_globus_prefix/include/$with_globus_nothr_flavor"
    GLOBUS_THR_CFLAGS="$with_globus_prefix/include/$with_globus_thr_flavor"

    ac_globus_ldlib="-L$with_globus_prefix/lib"

    GLOBUS_COMMON_NOTHR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_nothr_flavor"
    GLOBUS_COMMON_THR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_thr_flavor"

    GLOBUS_STATIC_COMMON_NOTHR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_nothr_flavor.a"
    GLOBUS_STATIC_COMMON_THR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_thr_flavor.a"

    GLOBUS_FTP_CLIENT_NOTHR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_nothr_flavor"
    GLOBUS_FTP_CLIENT_THR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_thr_flavor"

    GLOBUS_GSS_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_nothr_flavor -lglobus_gss_assist_$with_globus_nothr_flavor"
    GLOBUS_GSS_THR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_thr_flavor -lglobus_gss_assist_$with_globus_thr_flavor"

    GLOBUS_SSL_NOTHR_LIBS="$ac_globus_ldlib -lssl_$with_globus_nothr_flavor -lcrypto_$with_globus_nothr_flavor"
    GLOBUS_SSL_THR_LIBS="$ac_globus_ldlib -lssl_$with_globus_thr_flavor -lcrypto_$with_globus_thr_flavor"

    GLOBUS_STATIC_SSL_NOTHR_LIBS="$with_globus_prefix/lib/libssl_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libcrypto_$with_globus_nothr_flavor.a"
    GLOBUS_STATIC_SSL_THR_LIBS="$with_globus_prefix/lib/libssl_$with_globus_thr_flavor.a $with_globus_prefix/lib/libcrypto_$with_globus_thr_flavor.a"

    GLOBUS_LDAP_THR_LIBS="$ac_globus_ldlib -lldap_$with_globus_thr_flavor -llber_$with_globus_thr_flavor"

    dnl Needed by LCAS/LCMAPS voms plugins
    GLOBUS_GSI_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_nothr_flavor"
    GLOBUS_GSI_THR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_thr_flavor"

    dnl
    dnl check nothr openssl header
    dnl
    ac_globus_nothr_ssl="$with_globus_prefix/include/$with_globus_nothr_flavor/openssl"

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

    dnl
    dnl check thr openssl header
    dnl
    ac_globus_thr_ssl="$with_globus_prefix/include/$with_globus_thr_flavor/openssl"

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
    fi

    dnl
    dnl check thr ldap header
    dnl
    ac_globus_thr_ldap="$with_globus_prefix/include/$with_globus_thr_flavor"
                                                                                
    AC_MSG_CHECKING([for $ac_globus_thr_ldap/lber.h])
    
    if test ! -f "$ac_globus_thr_ldap/lber.h" ; then
	ac_globus_thr_ldap=""
	AC_MSG_RESULT([no])
    else
        AC_MSG_RESULT([yes])
    fi

    AC_MSG_CHECKING([for ldap thr])

    if test -n "$ac_globus_thr_ldap" ; then
        dnl
        dnl maybe do some complex test of globus instalation here later
        dnl
        ac_save_CFLAGS=$CFLAGS
        CFLAGS="$GLOBUS_THR_CFLAGS $CFLAGS"
	AC_TRY_COMPILE([
              #include "ldap.h"
              #include "lber.h"
           ],
           [
           LDAPMessage *ldresult;
           BerElement *ber;
           ],
           [ac_cv_globus_thr_valid2=yes],
           [ac_cv_globus_thr_valid2=no])
        CFLAGS=$ac_save_CFLAGS
        AC_MSG_RESULT([$ac_cv_globus_thr_valid2])
    fi

    if test x$ac_cv_globus_nothr_valid = xyes -a x$ac_cv_globus_thr_valid1 = xyes -a x$ac_cv_globus_thr_valid2 = xyes ; then
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

