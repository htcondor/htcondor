dnl Usage:
dnl AC_CLASSAD(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - CLASSAD_CFLAGS (compiler flags)
dnl - CLASSAD_LIBS (linker flags, stripping and path)
dnl - CLASSAD_DL_LIBS
dnl - CLASSAD_INSTALL_PATH
dnl - CLASSAD_PATH

AC_DEFUN(AC_CLASSADS,
[
    AC_ARG_WITH(classads_prefix, 
	[  --with-classads-prefix=PFX    prefix where the Classad is installed.],
	[], 
	with_classads_prefix="/opt/classads")

    AC_MSG_CHECKING([for CLASSAD installation])

    CLASSAD_CFLAGS=""
    CLASSAD_LIBS="-lclassad_ns"
    CLASSAD_DL_LIBS="-lclassad_dl"
    if test -n "$with_classads_prefix" -a "$with_classads_prefix" != "/usr" ; then
            AC_MSG_RESULT([prefix: $with_classads_prefix])

            ac_classads_prefix=$with_classads_prefix

            CLASSAD_CFLAGS="-I$with_classads_prefix/include"
            CLASSAD_LIBS="-L$with_classads_prefix/lib $CLASSAD_LIBS"
	    CLASSAD_DL_LIBS="-L$with_classads_prefix/lib $CLASSAD_DL_LIBS"
    fi

    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    ac_save_cppflags=$CPPFLAGS
    ac_save_libs=$LIBS
    CPPFLAGS="$CLASSAD_CFLAGS $CPPFLAGS"
    LIBS="$CLASSAD_LIBS $LIBS"
    AC_MSG_CHECKING([if a small classads program compiles])
    AC_TRY_LINK([ #include <classad/classad_distribution.h> ],
		[ classad::ClassAd ad; classad::ClassAdParser parser; ],
		[ ac_have_classads=yes ], [ ac_have_classads=no ])
    if test x$ac_have_classads = xno ; then
        CLASSAD_CFLAGS="$CLASSAD_CFLAGS -DWANT_CLASSAD_NAMESPACE"
        CPPFLAGS="$CLASSAD_CFLAGS $ac_save_cppflags"
        AC_TRY_LINK([ #include <classad/classad_distribution.h> ],
                    [ classad::ClassAd ad; classad::ClassAdParser parser; ],
                    [ ac_have_classads=yes ], [ ac_have_classads=no ])	
    fi
    AC_MSG_RESULT([$ac_have_classads])

    CPPFLAGS=$ac_save_cppflags
    LIBS=$ac_save_libs
    AC_LANG_RESTORE

    CLASSAD_PATH=$with_classads_prefix

    if test x$ac_have_classads = xyes ; then
        CLASSAD_INSTALL_PATH=$ac_classads_prefix
	ifelse([$2], , :, [$2])
    else
        AC_MSG_WARN([
	    ***  Cannot compile a small classads program: check whether the
	    ***  Condor ClassADs library is installed])
        CLASSAD_CFLAGS=""
        CLASSAD_LIBS=""
	CLASSAD_DL_LIBS=""
	CLASSAD_PATH=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(CLASSAD_INSTALL_PATH)
    AC_SUBST(CLASSAD_CFLAGS)
    AC_SUBST(CLASSAD_LIBS)
    AC_SUBST(CLASSAD_DL_LIBS)
    AC_SUBST(CLASSAD_PATH)
])

