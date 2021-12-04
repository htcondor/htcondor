dnl Usage:
dnl AC_GLITE_CEMONITOR_API
dnl - GLITE_CEMONITOR_CFLAGS
dnl - GLITE_CEMONITOR_LIBS

dnl Usage:
dnl AC_GLITE_CREAM_API
dnl - GLITE_CREAM_CFLAGS
dnl - GLITE_CREAM_LIBS

dnl Usage:
dnl AC_GLITE_CE_WSDL
dnl - GLITE_CREAM_WSDL
dnl - GLITE_CREAM_WSDL_DEPS
dnl - GLITE_CEMON_CLIENT_WSDL
dnl - GLITE_CEMON_CLIENT_WSDL_DEPS
dnl - GLITE_CEMON_CONSUMER_WSDL
dnl - GLITE_CEMON_CONSUMER_WSDL_DEPS

dnl Usage:
dnl AC_GLITE_ES_WSDL
dnl - GLITE_ES_WSDL
dnl - GLITE_ES_WSDL_DEPS

AC_DEFUN([AC_GLITE_CEMONITOR_API],
[
    ac_glite_ce_prefix=$GLITE_LOCATION

    have_cemon_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_CEMONITOR, monitor-client-api-c, have_cemon_pkgconfig=yes, have_cemon_pkgconfig=no)

    if test "x$have_cemon_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects CEMonitor definitions: $GLITE_CEMONITOR_CFLAGS $GLITE_CEMONITOR_LIBS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_ce_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib64"
        else
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib"
        fi

	GLITE_CEMONITOR_LIBS="$ac_glite_ce_lib -lglite_ce_monitor_client -lglite_ce_monitor_consumer"
        GLITE_CEMONITOR_CFLAGS="-I$ac_glite_ce_prefix/include -I$ac_glite_ce_prefix/include/glite/ce"
	ifelse([$2], , :, [$2])
    else
        GLITE_CEMONITOR_LIBS=""
	GLITE_CEMONITOR_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_CEMONITOR_CFLAGS)
    AC_SUBST(GLITE_CEMONITOR_LIBS)
])


AC_DEFUN([AC_GLITE_CREAM_API],
[
    ac_glite_ce_prefix=$GLITE_LOCATION

    have_cream_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_CREAM, cream-client-api-c, have_cream_pkgconfig=yes, have_cream_pkgconfig=no)

    if test "x$have_cream_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects CREAM definitions: $GLITE_CREAM_CFLAGS $GLITE_CREAM_LIBS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_ce_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib64"
        else
            ac_glite_ce_lib="-L$ac_glite_ce_prefix/lib"
        fi

        GLITE_CREAM_LIBS="$ac_glite_ce_lib -lglite_ce_cream_client_soap -lglite_ce_cream_client_util"
        GLITE_CREAM_CFLAGS="-I$ac_glite_ce_prefix/include -I$ac_glite_ce_prefix/include/glite/ce"
        ifelse([$2], , :, [$2])
    else
        GLITE_CREAM_LIBS=""
        GLITE_CREAM_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_CREAM_CFLAGS)
    AC_SUBST(GLITE_CREAM_LIBS)
])



AC_DEFUN([AC_GLITE_CE_WSDL],
[

    AC_ARG_WITH(ce_wsdl_location,
        [  --with-ce-wsdl-location=PATH    path of the wsdl file set],
        [],
        with_ce_wsdl_location=${GLITE_LOCATION:-/usr})

    GLITE_CE_WSDL_PATH=$with_ce_wsdl_location/share/wsdl/cream-ce
    AC_MSG_CHECKING([CREAM CE WSDL files in $with_ce_wsdl_location/share/wsdl/cream-ce])

    GLITE_CREAM_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-cream2_service.wsdl
    GLITE_CREAM_WSDL_DEPS="$GLITE_CREAM_WSDL \
                           $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd \
                           $GLITE_CE_WSDL_PATH/www.gridsite.org-delegation-2.0.0.wsdl"

    GLITE_CEMON_CLIENT_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-monitor_service.wsdl
    GLITE_CEMON_CLIENT_WSDL_DEPS="$GLITE_CEMON_CLIENT_WSDL \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_types.wsdl \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_faults.wsdl \
                                  $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd"

    GLITE_CEMON_CONSUMER_WSDL=$GLITE_CE_WSDL_PATH/org.glite.ce-monitor_consumer_service.wsdl
    GLITE_CEMON_CONSUMER_WSDL_DEPS="$GLITE_CEMON_CONSUMER_WSDL \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_types.wsdl \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-monitor_faults.wsdl \
                                    $GLITE_CE_WSDL_PATH/org.glite.ce-faults.xsd"

    if test -f "$GLITE_CREAM_WSDL" -a -f "$GLITE_CEMON_CLIENT_WSDL" ; then
        AC_SUBST(GLITE_CE_WSDL_PATH)
        AC_SUBST(GLITE_CREAM_WSDL)
        AC_SUBST(GLITE_CREAM_WSDL_DEPS)
        AC_SUBST(GLITE_CEMON_CLIENT_WSDL)
        AC_SUBST(GLITE_CEMON_CLIENT_WSDL_DEPS)
        AC_SUBST(GLITE_CEMON_CONSUMER_WSDL)
        AC_SUBST(GLITE_CEMON_CONSUMER_WSDL_DEPS)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([no])])
    fi
])




AC_DEFUN([AC_GLITE_ES_WSDL],
[

    AC_ARG_WITH(es_wsdl_location,
        [  --with-es-wsdl-location=PATH    path of the wsdl file set],
        [],
        with_es_wsdl_location=${GLITE_LOCATION:-/usr})

    GLITE_ES_WSDL_PATH=$with_es_wsdl_location/share/wsdl/cream-ce/es
    AC_MSG_CHECKING([ES CE WSDL files in $with_es_wsdl_location/share/wsdl/cream-ce/es])

    GLITE_ES_WSDL="$GLITE_ES_WSDL_PATH/Creation.wsdl \
			$GLITE_ES_WSDL_PATH/ActivityManagement.wsdl \
			$GLITE_ES_WSDL_PATH/ActivityInfo.wsdl \
			$GLITE_ES_WSDL_PATH/Delegation.wsdl \
			$GLITE_ES_WSDL_PATH/ResourceInfo.wsdl"

    GLITE_ES_WSDL_DEPS="$GLITE_ES_WSDL \
			$GLITE_ES_WSDL_PATH/ActivityInfo.xsd \
			$GLITE_ES_WSDL_PATH/ActivityManagement.xsd \
			$GLITE_ES_WSDL_PATH/Creation.xsd \
			$GLITE_ES_WSDL_PATH/Delegation.xsd \
			$GLITE_ES_WSDL_PATH/es-activity-description.xsd \
			$GLITE_ES_WSDL_PATH/es-main.xsd \
			$GLITE_ES_WSDL_PATH/GLUE2.xsd \
			$GLITE_ES_WSDL_PATH/ResourceInfo.xsd"

    file_missing=true
    for item in $GLITE_ES_WSDL_DEPS ; do
        if test -f $item ; then
            file_missing=false
        fi
    done

    if test "x$file_missing" == "xfalse"; then
        AC_SUBST(GLITE_ES_WSDL_PATH)
        AC_SUBST(GLITE_ES_WSDL)
        AC_SUBST(GLITE_ES_WSDL_DEPS)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([no])])
    fi
])


