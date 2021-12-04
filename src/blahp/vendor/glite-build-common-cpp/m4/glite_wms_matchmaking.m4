dnl Usage:
dnl AC_GLITE_WMS_MATCHMAING
dnl - GLITE_WMS_MATCHMAKING_CFLAGS
dnl - GLITE_WMS_MATHCMAKING_LIBS

AC_DEFUN([AC_GLITE_WMS_MATCHMAKING],
[
    ac_glite_wms_match_prefix=$GLITE_LOCATION

    have_mm_pkgconfig=no
    PKG_CHECK_MODULES(GLITE_WMS_MATCHMAKING, wms-matchmaking, have_mm_pkgconfig=yes, have_mm_pkgconfig=no)

    if test "x$have_mm_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects definitions for WMS Matchmaking])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wms_match_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_wms_match_lib="-L$ac_glite_wms_match_prefix/lib64"
        else
            ac_glite_wms_match_lib="-L$ac_glite_wms_match_prefix/lib"
        fi

    	GLITE_WMS_MATCHMAKING_CFLAGS="-I$ac_glite_wms_match_prefix/include"
	GLITE_WMS_MATCHMAKING_LIBS="$ac_glite_wms_match_lib -lglite_wms_matchmaking"
	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_MATCHMAKING_CFLAGS=""
	GLITE_WMS_MATCHMAKING_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_MATCHMAKING_CFLAGS)
    AC_SUBST(GLITE_WMS_MATCHMAKING_LIBS)
])

