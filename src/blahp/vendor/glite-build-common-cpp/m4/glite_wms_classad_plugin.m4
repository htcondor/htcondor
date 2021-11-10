dnl Usage:
dnl AC_GLITE_WMS_CLASSAD_PLUGIN
dnl - GLITE_WMS_CLASSAD_PLUGIN_CFLAGS
dnl - GLITE_WMS_CLASSAD_PLUGIN_LIBS

AC_DEFUN([AC_GLITE_WMS_CLASSAD_PLUGIN],
[
    ac_glite_wms_cp_prefix=$GLITE_LOCATION

    have_classadp_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_CLASSAD_PLUGIN, wms-classad-plugin,  have_classadp_pkgconfig=yes, have_classadp_pkgconfig=no)

    if test "x$have_classadp_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for classad plugin])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_cp_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_cp_lib="-L$ac_glite_wms_cp_prefix/lib64"
        else
            ac_glite_wms_cp_lib="-L$ac_glite_wms_cp_prefix/lib"
	fi

        GLITE_WMS_CLASSAD_PLUGIN_CFLAGS="-I$ac_glite_wms_cp_prefix/include"
        GLITE_WMS_CLASSAD_PLUGIN_LIBS="$ac_glite_wms_cp_lib -lglite_wms_classad_plugin_loader"
	ifelse([$2], , :, [$2])
    else
        GLITE_WMS_CLASSAD_PLUGIN_CFLAGS=""
	GLITE_WMS_CLASSAD_PLUGIN_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_CLASSAD_PLUGIN_CFLAGS)
    AC_SUBST(GLITE_WMS_CLASSAD_PLUGIN_LIBS)
])

