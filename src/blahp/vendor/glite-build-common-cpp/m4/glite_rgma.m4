dnl Usage:
dnl AC_GLITE_RGMA
dnl - GLITE_RGMA_API_LIBS

AC_DEFUN([AC_GLITE_RGMA],
[
    ac_glite_rgma_prefix=$GLITE_LOCATION

    if test -n "ac_glite_rgma_prefix" ; then
	dnl
	dnl 
	dnl
        ac_glite_rgma_lib="-L$ac_glite_rgma_prefix/lib"
	GLITE_RGMA_API_LIBS="$ac_glite_rgma_lib -lglite-rgma-cpp-impl -lglite-rgma-cpp"
	ifelse([$2], , :, [$2])
    else
	GLITE_RGMA_API_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_RGMA_API_LIBS)
])

