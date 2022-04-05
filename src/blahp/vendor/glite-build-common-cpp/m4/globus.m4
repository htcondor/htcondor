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
    have_globus_flavored=no
    AC_GLOBUS_FLAVORED([],have_globus_flavored=yes,have_globus_flavored=no)

    if test "x$have_globus_flavored" = "xyes" ; then
        ifelse([$2], , :, [$2])
    else
        have_globus_pkgconfig=no
        AC_GLOBUS_PKGCONFIG([], have_globus_pkgconfig=yes, have_globus_pkgconfig=no)

        if test "x$have_globus_pkgconfig" = "xyes" ; then
            ifelse([$2], , :, [$2])
        else
            ifelse([$3], , :, [$3])
        fi
    fi

])

AC_DEFUN([AC_GLOBUS_FLAVORED],
[
    AC_ARG_WITH(globus_prefix,
    [  --with-globus-prefix=PFX     prefix where GLOBUS is installed. (/usr)],
    [],
        with_globus_prefix=${GLOBUS_LOCATION:-/usr})

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

    GLOBUS_NOTHR_CFLAGS="-I$with_globus_prefix/include/$with_globus_nothr_flavor"
    GLOBUS_THR_CFLAGS="-I$with_globus_prefix/include/$with_globus_thr_flavor"

    ac_globus_ldlib="-L$with_globus_prefix/lib"

    GLOBUS_COMMON_NOTHR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_nothr_flavor"
    GLOBUS_COMMON_THR_LIBS="$ac_globus_ldlib -lglobus_common_$with_globus_thr_flavor"

    GLOBUS_STATIC_COMMON_NOTHR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_nothr_flavor.a"
    GLOBUS_STATIC_COMMON_THR_LIBS="$with_globus_prefix/lib/libglobus_common_$with_globus_thr_flavor.a"

    GLOBUS_FTP_CLIENT_NOTHR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_nothr_flavor"
    GLOBUS_FTP_CLIENT_THR_LIBS="$ac_globus_ldlib -lglobus_ftp_client_$with_globus_thr_flavor"

    GLOBUS_GSS_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_nothr_flavor -lglobus_gss_assist_$with_globus_nothr_flavor"
    GLOBUS_GSS_THR_LIBS="$ac_globus_ldlib -lglobus_gssapi_gsi_$with_globus_thr_flavor -lglobus_gss_assist_$with_globus_thr_flavor"

    GLOBUS_LDAP_THR_LIBS="$ac_globus_ldlib -lldap_$with_globus_thr_flavor -llber_$with_globus_thr_flavor"

    dnl Needed by LCAS/LCMAPS voms plugins
    GLOBUS_GSI_NOTHR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_nothr_flavor"
    GLOBUS_GSI_THR_LIBS="$ac_globus_ldlib -lglobus_gsi_credential_$with_globus_thr_flavor"

    dnl
    dnl check whether globus in place, if not return error
    dnl
    AC_MSG_CHECKING([for globus version])
    if test -f $with_globus_prefix/bin/globus-version; then
        grep GLOBUS_VERSION= $with_globus_prefix/bin/globus-version | cut -d'"' -f2 >& globus.version
        ac_globus_version=`cat globus.version`
        ac_globus_point_version=`cut -d. -f3 globus.version`
        ac_globus_minor_version=`cut -d. -f2 globus.version`
        ac_globus_major_version=`cut -d. -f1 globus.version`
    fi

    if test -n "$ac_globus_point_version" ; then
        AC_MSG_RESULT([$ac_globus_version])
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
        echo ac_point_version $ac_globus_point_version
        AC_MSG_RESULT([no])
    fi


    dnl
    dnl only perform ssl checks if globus <= 4.0.6 (include VDT-1.6.1 and 1.8.1)
    dnl
    if test [ $ac_globus_point_version -le 6 -a $ac_globus_minor_version -eq 0 -a $ac_globus_major_version -eq 4 ] ; then
        dnl
        dnl check nothr openssl header
        dnl
        GLOBUS_SSL_NOTHR_LIBS="$ac_globus_ldlib -lssl_$with_globus_nothr_flavor -lcrypto_$with_globus_nothr_flavor"
        GLOBUS_SSL_THR_LIBS="$ac_globus_ldlib -lssl_$with_globus_thr_flavor -lcrypto_$with_globus_thr_flavor"
        GLOBUS_STATIC_SSL_NOTHR_LIBS="$with_globus_prefix/lib/libssl_$with_globus_nothr_flavor.a $with_globus_prefix/lib/libcrypto_$with_globus_nothr_flavor.a"
        GLOBUS_STATIC_SSL_THR_LIBS="$with_globus_prefix/lib/libssl_$with_globus_thr_flavor.a $with_globus_prefix/lib/libcrypto_$with_globus_thr_flavor.a"

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
            GLOBUS_NOTHR_CFLAGS="-I$ac_globus_nothr_ssl $GLOBUS_NOTHR_CFLAGS"
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
            GLOBUS_THR_CFLAGS="-I$ac_globus_thr_ssl $GLOBUS_THR_CFLAGS"
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
    fi
    
    if test "x$HOSTTYPE" = "xx86_64"; then
        dnl
        dnl Temporarily remove this check on x86_64 since ldap is not available in VDT
        dnl

        ac_cv_globus_thr_valid2="yes"
        GLOBUS_LDAP_THR_LIBS=""
    else
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
    fi
    
    dnl
    dnl only perform ssl checks if globus <= 4.0.6 (include VDT-1.6.1 and 1.8.1)
    dnl
    if test [ $ac_globus_point_version -ge 7 -o $ac_globus_minor_version -ge 1 -o $ac_globus_major_version -ge 5 ] ; then
        GLOBUS_LOCATION=$with_globus_prefix
        GLOBUS_NOTHR_FLAVOR=$with_globus_nothr_flavor
        GLOBUS_THR_FLAVOR=$with_globus_thr_flavor
        GLOBUS_SSL_NOTHR_LIBS=""
        GLOBUS_SSL_THR_LIBS=""
        GLOBUS_STATIC_SSL_NOTHR_LIBS=""
        GLOBUS_STATIC_SSL_THR_LIBS=""
        AC_MSG_RESULT([Set globus environment variables and unset SSL variables])
        ifelse([$2], , :, [$2])
    else
        if test x$ac_cv_globus_nothr_valid = xyes -a x$ac_cv_globus_thr_valid1 = xyes -a x$ac_cv_globus_thr_valid2 = xyes ; then
            GLOBUS_LOCATION=$with_globus_prefix
            GLOBUS_NOTHR_FLAVOR=$with_globus_nothr_flavor
            GLOBUS_THR_FLAVOR=$with_globus_thr_flavor
            AC_MSG_RESULT([All 3 globus tests pass])
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
            AC_MSG_WARN([No all 3 globus tests pass unset variables])
            ifelse([$3], , :, [$3])
        fi
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

dnl Usage:
dnl AC_GLOBUS_PKGCONFIG(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - GLOBUS_THR_CFLAGS
dnl - GLOBUS_THR_LIBS
dnl - GLOBUS_COMMON_THR_LIBS
dnl - GLOBUS_FTP_CLIENT_THR_LIBS
dnl - GLOBUS_SSL_THR_LIBS
dnl - GLOBUS_GSS_THR_LIBS
dnl - GLOBUS_LDAP_THR_LIBS

AC_DEFUN([AC_GLOBUS_PKGCONFIG],
[

    have_globus_callout=no
    PKG_CHECK_MODULES(GLOBUS_CALLOUT, globus-callout, have_globus_callout=yes, have_globus_callout=no)
    
    have_globus_common=no
    PKG_CHECK_MODULES(GLOBUS_COMMON, globus-common, have_globus_common=yes, have_globus_common=no)
    
    have_globus_core=no
    PKG_CHECK_MODULES(GLOBUS_CORE, globus-core, have_globus_core=yes, have_globus_core=no)
    
    have_globus_ftp_client=no
    PKG_CHECK_MODULES(GLOBUS_FTP_CLIENT, globus-ftp-client, have_globus_ftp_client=yes, have_globus_ftp_client=no)
    
    have_globus_ftp_control=no
    PKG_CHECK_MODULES(GLOBUS_FTP_CONTROL, globus-ftp-control, have_globus_ftp_control=yes, have_globus_ftp_control=no)
    
    have_globus_gsi_callback=no
    PKG_CHECK_MODULES(GLOBUS_GSI_CALLBACK, globus-gsi-callback, have_globus_gsi_callback=yes, have_globus_gsi_callback=no)
    
    have_globus_gsi_cert_utils=no
    PKG_CHECK_MODULES(GLOBUS_GSI_CERT_UTILS, globus-gsi-cert-utils, have_globus_gsi_cert_utils=yes, have_globus_gsi_cert_utils=no)
    
    have_globus_gsi_credential=no
    PKG_CHECK_MODULES(GLOBUS_GSI_CREDENTIAL, globus-gsi-credential, have_globus_gsi_credential=yes, have_globus_gsi_credential=no)
    
    have_globus_gsi_openssl_error=no
    PKG_CHECK_MODULES(GLOBUS_GSI_OPENSSL_ERROR, globus-gsi-openssl-error, have_globus_gsi_openssl_error=yes, have_globus_gsi_openssl_error=no)
    
    have_globus_gsi_proxy_core=no
    PKG_CHECK_MODULES(GLOBUS_GSI_PROXY_CORE, globus-gsi-proxy-core, have_globus_gsi_proxy_core=yes, have_globus_gsi_proxy_core=no)
    
    have_globus_gsi_proxy_ssl=no
    PKG_CHECK_MODULES(GLOBUS_GSI_PROXY_SSL, globus-gsi-proxy-ssl, have_globus_gsi_proxy_ssl=yes, have_globus_gsi_proxy_ssl=no)
    
    have_globus_gsi_sysconfig=no
    PKG_CHECK_MODULES(GLOBUS_GSI_SYSCONFIG, globus-gsi-sysconfig, have_globus_gsi_sysconfig=yes, have_globus_gsi_sysconfig=no)
    
    have_globus_gssapi_error=no
    PKG_CHECK_MODULES(GLOBUS_GSSAPI_ERROR, globus-gssapi-error, have_globus_gssapi_error=yes, have_globus_gssapi_error=no)
    
    have_globus_gssapi_gsi=no
    PKG_CHECK_MODULES(GLOBUS_GSSAPI_GSI, globus-gssapi-gsi, have_globus_gssapi_gsi=yes, have_globus_gssapi_gsi=no)
    
    have_globus_gss_assist=no
    PKG_CHECK_MODULES(GLOBUS_GSS_ASSIST, globus-gss-assist, have_globus_gss_assist=yes, have_globus_gss_assist=no)
    
    have_globus_io=no
    PKG_CHECK_MODULES(GLOBUS_IO, globus-io, have_globus_io=yes, have_globus_io=no)
    
    have_globus_openssl_module=no
    PKG_CHECK_MODULES(GLOBUS_OPENSSL_MODULE, globus-openssl-module, have_globus_openssl_module=yes, have_globus_openssl_module=no)
    
    have_globus_openssl=no
    PKG_CHECK_MODULES(GLOBUS_OPENSSL, globus-openssl, have_globus_openssl=yes, have_globus_openssl=no)
    
    have_globus_xio=no
    PKG_CHECK_MODULES(GLOBUS_XIO, globus-xio, have_globus_xio=yes, have_globus_xio=no)


    GLOBUS_FTP_CLIENT_THR_LIBS="${GLOBUS_FTP_CLIENT_LIBS} ${GLOBUS_FTP_CONTROL_LIBS}"
    GLOBUS_SSL_THR_LIBS="${GLOBUS_OPENSSL_LIBS} ${GLOBUS_OPENSSL_MODULE_LIBS}"
    GLOBUS_COMMON_THR_LIBS="${GLOBUS_CALLOUT_LIBS} ${GLOBUS_COMMON_LIBS} ${GLOBUS_CORE_LIBS}"
    
    GLOBUS_GSS_THR_LIBS="${GLOBUS_GSI_CALLBACK_LIBS} ${GLOBUS_GSI_CERT_UTILS_LIBS} ${GLOBUS_GSI_CREDENTIAL_LIBS}"
    GLOBUS_GSS_THR_LIBS="${GLOBUS_GSS_THR_LIBS} ${GLOBUS_GSI_OPENSSL_ERROR_LIBS} ${GLOBUS_GSI_PROXY_CORE_LIBS}"
    GLOBUS_GSS_THR_LIBS="${GLOBUS_GSS_THR_LIBS} ${GLOBUS_GSI_PROXY_SSL_LIBS} ${GLOBUS_GSI_SYSCONFIG_SSL}"
    GLOBUS_GSS_THR_LIBS="${GLOBUS_GSS_THR_LIBS} ${GLOBUS_GSSAPI_ERROR_LIBS} ${GLOBUS_GSSAPI_GSI_LIBS} ${GLOBUS_GSS_ASSIST_LIBS}"

    GLOBUS_THR_LIBS="${GLOBUS_FTP_CLIENT_THR_LIBS} ${GLOBUS_SSL_THR_LIBS} ${GLOBUS_COMMON_THR_LIBS}"
    GLOBUS_THR_LIBS="${GLOBUS_THR_LIBS} ${GLOBUS_IO_LIBS} ${GLOBUS_XIO_LIBS}"
    
    
    GLOBUS_FTP_CLIENT_THR_CFLAGS="${GLOBUS_FTP_CLIENT_CFLAGS} ${GLOBUS_FTP_CONTROL_CFLAGS}"
    GLOBUS_SSL_THR_CFLAGS="${GLOBUS_OPENSSL_CFLAGS} ${GLOBUS_OPENSSL_MODULE_CFLAGS}"
    GLOBUS_COMMON_THR_CFLAGS="${GLOBUS_CALLOUT_CFLAGS} ${GLOBUS_COMMON_CFLAGS} ${GLOBUS_CORE_CFLAGS}"
    
    GLOBUS_GSS_THR_CFLAGS="${GLOBUS_GSI_CALLBACK_CFLAGS} ${GLOBUS_GSI_CERT_UTILS_CFLAGS} ${GLOBUS_GSI_CREDENTIAL_CFLAGS}"
    GLOBUS_GSS_THR_CFLAGS="${GLOBUS_GSS_THR_CFLAGS} ${GLOBUS_GSI_OPENSSL_ERROR_CFLAGS} ${GLOBUS_GSI_PROXY_CORE_CFLAGS}"
    GLOBUS_GSS_THR_CFLAGS="${GLOBUS_GSS_THR_CFLAGS} ${GLOBUS_GSI_PROXY_SSL_CFLAGS} ${GLOBUS_GSI_SYSCONFIG_SSL}"
    GLOBUS_GSS_THR_CFLAGS="${GLOBUS_GSS_THR_CFLAGS} ${GLOBUS_GSSAPI_ERROR_CFLAGS} ${GLOBUS_GSSAPI_GSI_CFLAGS} ${GLOBUS_GSS_ASSIST_CFLAGS}"

    GLOBUS_THR_CFLAGS="${GLOBUS_FTP_CLIENT_THR_CFLAGS} ${GLOBUS_SSL_THR_CFLAGS} ${GLOBUS_COMMON_THR_CFLAGS}"
    GLOBUS_THR_CFLAGS="${GLOBUS_THR_CFLAGS} ${GLOBUS_IO_CFLAGS} ${GLOBUS_XIO_CFLAGS}"
    
    if test x$have_globus_callout = xyes -a x$have_globus_common = xyes -a x$have_globus_core = xyes \
            -a x$have_globus_ftp_client = xyes -a x$have_globus_ftp_control = xyes -a x$have_globus_gsi_callback = xyes \
            -a x$have_globus_gsi_cert_utils = xyes -a x$have_globus_gsi_credential = xyes \
            -a x$have_globus_gsi_openssl_error = xyes -a x$have_globus_gsi_proxy_core = xyes \
            -a x$have_globus_gsi_proxy_ssl = xyes -a x$have_globus_gsi_sysconfig = xyes \
            -a x$have_globus_gssapi_error = xyes -a x$have_globus_gssapi_gsi = xyes -a x$have_globus_gss_assist = xyes \
            -a x$have_globus_io = xyes -a x$have_globus_openssl_module = xyes -a x$have_globus_openssl = xyes \
            -a x$have_globus_xio = xyes ; then
        ifelse([$2], , :, [$2])
    else
        ifelse([$3], , :, [$3])
    fi

    GLOBUS_NOTHR_CFLAGS=$GLOBUS_THR_CFLAGS
    GLOBUS_NOTHR_LIBS=$GLOBUS_THR_LIBS
    GLOBUS_COMMON_NOTHR_LIBS=$GLOBUS_COMMON_THR_LIBS
    GLOBUS_FTP_CLIENT_NOTHR_LIBS=$GLOBUS_FTP_CLIENT_THR_LIBS
    GLOBUS_SSL_NOTHR_LIBS=$GLOBUS_SSL_THR_LIBS
    GLOBUS_GSS_NOTHR_LIBS=$GLOBUS_GSS_THR_LIBS
    GLOBUS_LDAP_NOTHR_LIBS=$GLOBUS_LDAP_THR_LIBS

    AC_SUBST(GLOBUS_THR_CFLAGS)
    AC_SUBST(GLOBUS_THR_LIBS)
    AC_SUBST(GLOBUS_COMMON_THR_LIBS)
    AC_SUBST(GLOBUS_FTP_CLIENT_THR_LIBS)
    AC_SUBST(GLOBUS_SSL_THR_LIBS)
    AC_SUBST(GLOBUS_GSS_THR_LIBS)
    AC_SUBST(GLOBUS_LDAP_THR_LIBS)

    AC_SUBST(GLOBUS_NOTHR_CFLAGS)
    AC_SUBST(GLOBUS_NOTHR_LIBS)
    AC_SUBST(GLOBUS_COMMON_NOTHR_LIBS)
    AC_SUBST(GLOBUS_FTP_CLIENT_NOTHR_LIBS)
    AC_SUBST(GLOBUS_SSL_NOTHR_LIBS)
    AC_SUBST(GLOBUS_GSS_NOTHR_LIBS)
    AC_SUBST(GLOBUS_LDAP_NOTHR_LIBS)

])
