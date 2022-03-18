dnl Usage:
dnl AC_GLITE_JDL
dnl - GLITE_JDL_LIBS

AC_DEFUN([AC_GLITE_JDL],
[
    ac_glite_jdl_prefix=$GLITE_LOCATION

    have_jdl_pkconfig=no
    PKG_CHECK_MODULES(GLITE_JDL, jdl-api-cpp, have_jdl_pkgconfig=yes, have_jdl_pkgconfig=no)
    
    if test "x$have_jdl_pkgconfig" = "xyes" ; then
        AC_MSG_RESULT([pkg-config detects JDL definitions: $GLITE_JDL_CFLAGS $GLITE_JDL_LIBS])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_jdl_prefix" ; then
        if test "x$host_cpu" = "xx86_64"; then
            ac_glite_jdl_lib="-L$ac_glite_jdl_prefix/lib64"
        else
            ac_glite_jdl_lib="-L$ac_glite_jdl_prefix/lib"
        fi
    	GLITE_JDL_CFLAGS="-I$ac_glite_jdl_prefix/include -I$ac_glite_jdl_prefix/include/glite/jdl"
	GLITE_JDL_LIBS="$ac_glite_jdl_lib -lglite_jdl_api_cpp"
	ifelse([$2], , :, [$2])
    else
	GLITE_JDL_CFLAGS=""
	GLITE_JDL_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_JDL_CFLAGS)
    AC_SUBST(GLITE_JDL_LIBS)
])

