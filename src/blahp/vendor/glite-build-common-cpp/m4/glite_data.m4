dnl Usage:
dnl AC_GLITE_DATA(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - GLITE_DATA_CFLAGS (compiler flags)
dnl - GLITE_DATA_LIBS (linker flags, stripping and path)
dnl prerequisites:

AC_DEFUN([AC_GLITE_DATA],
[

    ac_glite_data_prefix=$GLITE_LOCATION

    if test -n "ac_glite_data_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_data_lib="-L$ac_glite_data_prefix/lib"

        GLITE_DATA_LIBS="$ac_glite_data_prefix/lib/libglite_data_catalog_storageindex_api_c.a"
	ifelse([$2], , :, [$2])
    else
        GLITE_DATA_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_DATA_CFLAGS)
    AC_SUBST(GLITE_DATA_LIBS)
])

