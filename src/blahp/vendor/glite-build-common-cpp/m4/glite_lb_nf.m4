dnl Usage:
dnl AC_GLITE_LB_NF
dnl - GLITE_LB_CFLAGS
dnl - GLITE_LB_CLIENT_LIBS
dnl - GLITE_LB_CLIENTPP_LIBS
dnl - GLITE_LB_COMMON_LIBS

AC_DEFUN([AC_GLITE_LB_NF],
[
    AC_ARG_WITH(lb-prefix,
        [  --with-lb-prefix=PFX     prefix where proxyrenewal is installed. (/usr)],
        [],
        with_lb_prefix=${GLITE_LOCATION:-/usr})

    if test "x$host_cpu" = "xx86_64"; then
        library_path="lib64"
    else
        library_path="lib"
    fi

    if test -h "/usr/lib64" ; then
        library_path="lib"
    fi

    if ! test -e "/usr/lib64" ; then
        library_path="lib"
    fi

    if test -n "with_lb_prefix" ; then
        GLITE_LB_CFLAGS="-I$with_lb_prefix/include"
        ac_glite_lb_lib="-L$with_lb_prefix/$library_path"
	GLITE_LB_CLIENT_LIBS="$ac_glite_lb_lib -lglite_lb_client"
	GLITE_LB_CLIENTPP_LIBS="$ac_glite_lb_lib -lglite_lb_clientpp"
	GLITE_LB_COMMON_LIBS="$ac_glite_lb_lib -lglite_lb_common -lglite_lbu_trio -lglite_security_gss"
	ifelse([$2], , :, [$2])
    else
        GLITE_LB_CFLAGS=""
	GLITE_LB_CLIENT_LIBS=""
	GLITE_LB_CLIENTPP_LIBS=""
	GLITE_LB_COMMON_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_LB_CFLAGS)
    AC_SUBST(GLITE_LB_CLIENT_LIBS)
    AC_SUBST(GLITE_LB_CLIENTPP_LIBS)
    AC_SUBST(GLITE_LB_COMMON_LIBS)
])

