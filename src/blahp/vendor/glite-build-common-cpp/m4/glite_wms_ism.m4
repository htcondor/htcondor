dnl Usage:
dnl AC_GLITE_WMS_ISM
dnl - GLITE_WMS_ISM_LIBS
dnl - GLITE_WMS_ISM_II_PURCHASER_LIBS
dnl - GLITE_WMS_ISM_CEMON_PURCHASER_LIBS

AC_DEFUN([AC_GLITE_WMS_ISM],
[
    ac_glite_wms_ism_prefix=$GLITE_LOCATION

    have_ism_pkgconfig=yes
    PKG_CHECK_MODULES(GLITE_WMS_ISM, wms-ism, , have_ism_pkgconfig=no)
    PKG_CHECK_MODULES(GLITE_WMS_ISM_II_PURCHASER, wms-ism-ii-purchaser, , have_ism_pkgconfig=no)

    if test "x$have_ism_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS ISM])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_ism_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_ism_lib="-L$ac_glite_wms_ism_prefix/lib64"
        else
            ac_glite_wms_ism_lib="-L$ac_glite_wms_ism_prefix/lib"
        fi

	GLITE_WMS_ISM_LIBS="$ac_glite_wms_ism_lib -lglite_wms_ism"
        GLITE_WMS_ISM_CFLAGS="-I$ac_glite_wms_ism_prefix/include"
	GLITE_WMS_ISM_II_PURCHASER_LIBS="$ac_glite_wms_ism_lib -lglite_wms_ism_ii_purchaser"
        GLITE_WMS_ISM_II_PURCHASER_CFLAGS="-I$ac_glite_wms_ism_prefix/include"
dnl        GLITE_WMS_ISM_CEMON_PURCHASER_LIBS="$ac_glite_wms_ism_lib -lglite_wms_ism_cemon_purchaser"
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_ISM_LIBS=""
        GLITE_WMS_ISM_CFLAGS=""
	GLITE_WMS_ISM_II_PURCHASER_LIBS=""
        GLITE_WMS_ISM_II_PURCHASER_CFLAGS=""
dnl	GLITE_WMS_ISM_CEMON_PURCHASER_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_ISM_LIBS)
    AC_SUBST(GLITE_WMS_ISM_CFLAGS)
    AC_SUBST(GLITE_WMS_ISM_II_PURCHASER_LIBS)
    AC_SUBST(GLITE_WMS_ISM_II_PURCHASER_CFLAGS)
dnl    AC_SUBST(GLITE_WMS_ISM_CEMON_PURCHASER_LIBS)
])

