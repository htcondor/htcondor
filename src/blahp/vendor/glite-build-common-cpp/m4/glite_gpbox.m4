dnl Usage:
dnl AC_GLITE_GPBOX
dnl - GLITE_GPBOX_LIBS

AC_DEFUN([AC_GLITE_GPBOX],
[
    ac_glite_gpbox_prefix=$GLITE_LOCATION

    if test -n "ac_glite_gpbox_prefix" ; then
	dnl
	dnl 
	dnl
        ac_glite_gpbox_lib="-L$ac_glite_gpbox_prefix/lib"
	GLITE_GPBOX_LIBS="$ac_glite_gpbox_lib -lpep -lpci"

	ifelse([$2], , :, [$2])
    else
	GLITE_GPBOX_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_GPBOX_LIBS)
])

