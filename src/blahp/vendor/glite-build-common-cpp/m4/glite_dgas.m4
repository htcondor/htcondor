dnl Usage:
dnl AC_GLITE_DGAS
dnl - GLITE_DGAS_ATMBANKCLIENT_LIBS
dnl - GLITE_DGAS_ATMCLIENT_LIBS
dnl - GLITE_DGAS_HLR_LIBS
dnl - GLITE_DGAS_HLRJOBAUTH_LIBS
dnl - GLITE_DGAS_HLRQTRANSACTION_LIBS
dnl - GLITE_DGAS_HLRTRANSLOG_LIBS
dnl - GLITE_DGAS_JOBAUTHCLIENT_LIBS
dnl - GLITE_DGAS_PACLIENT_LIBS
dnl - GLITE_DGAS_PINGCLIENT

AC_DEFUN([AC_GLITE_DGAS],
[
    ac_glite_dgas_prefix=$GLITE_LOCATION

    if test -n "ac_glite_dgas_prefix" ; then
        dnl
	dnl 
	dnl
	if test "x$host_cpu" = "xx86_64"; then
	    ac_glite_dgas_lib="-L$ac_glite_dgas_prefix/lib64"
	else
	    ac_glite_dgas_lib="-L$ac_glite_dgas_prefix/lib" 
	fi
	
	if test -h "/usr/lib64"; then
	    ac_glite_dgas_lib="-L$ac_glite_dgas_prefix/lib" 
	fi

	if ! test -e "/usr/lib64"; then
	    ac_glite_dgas_lib="-L$ac_glite_dgas_prefix/lib" 
	fi

	GLITE_DGAS_ATMBANKCLIENT_LIBS="$ac_glite_dgas_lib -lglite_dgas_atmBankClient"
	GLITE_DGAS_ATMCLIENT_LIBS="$ac_glite_dgas_lib -lglite_dgas_atmClient"
	GLITE_DGAS_HLR_LIBS="$ac_glite_dgas_lib -lglite_dgas_hlr"
	GLITE_DGAS_HLRJOBAUTH_LIBS="$ac_glite_dgas_lib -lglite_dgas_hlrJobAuth"
	GLITE_DGAS_HLRQTRANSACTION_LIBS="$ac_glite_dgas_lib -lglite_dgas_hlrQTransaction"
	GLITE_DGAS_HLRTRANSLOG_LIBS="$ac_glite_dgas_lib -lglite_dgas_hlrTransLog"
	
	GLITE_DGAS_JOBAUTHCLIENT_LIBS="$ac_glite_dgas_lib -lglite_dgas_jobAuthClient"
	GLITE_DGAS_PACLIENT_LIBS="$ac_glite_dgas_lib -lglite_dgas_paClient"
	GLITE_DGAS_PINGCLIENT="$ac_glite_dgas_lib -lglite_dgas_pingClient"
	
	ifelse([$2], , :, [$2])
    else
	GLITE_DGAS_ATMBANKCLIENT_LIBS=""
	GLITE_DGAS_ATMCLIENT_LIBS=""
	GLITE_DGAS_HLR_LIBS=""
	GLITE_DGAS_HLRJOBAUTH_LIBS=""
	GLITE_DGAS_HLRQTRANSACTION_LIBS=""
	GLITE_DGAS_HLRTRANSLOG_LIBS=""
	GLITE_DGAS_JOBAUTHCLIENT_LIBS=""
	GLITE_DGAS_PACLIENT_LIBS=""
	GLITE_DGAS_PINGCLIENT=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_DGAS_ATMBANKCLIENT_LIBS)
    AC_SUBST(GLITE_DGAS_ATMCLIENT_LIBS)
    AC_SUBST(GLITE_DGAS_HLR__LIBS)
    AC_SUBST(GLITE_DGAS_HLRJOBAUTH_LIBS)
    AC_SUBST(GLITE_DGAS_HLRQTRANSACTION_LIBS)
    AC_SUBST(GLITE_DGAS_HLRTRANSLOG_LIBS)
    AC_SUBST(GLITE_DGAS_JOBAUTHCLIENT_LIBS)
    AC_SUBST(GLITE_DGAS_PACLIENT_LIBS)
    AC_SUBST(GLITE_DGAS_PINGCLIENT)
])

