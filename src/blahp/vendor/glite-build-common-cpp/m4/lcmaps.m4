dnl Usage:
dnl AC_LCMAPS(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl    LCMAPS_GLOBUS_NOTHR_CFLAGS
dnl    LCMAPS_GLOBUS_THR_CFLAGS
dnl    LCMAPS_GLOBUS_GSS_NOTHR_LIBS
dnl    LCMAPS_GLOBUS_GSS_THR_LIBS
dnl    LCMAPS_GLOBUS_GSI_NOTHR_LIBS
dnl    LCMAPS_GLOBUS_GSI_THR_LIBS

AC_DEFUN([AC_LCMAPS],
[
    LCMAPS_GLOBUS_NOTHR_CFLAGS=""
    LCMAPS_GLOBUS_THR_CFLAGS=""
    LCMAPS_GLOBUS_GSS_NOTHR_LIBS=""
    LCMAPS_GLOBUS_GSS_THR_LIBS=""
    LCMAPS_GLOBUS_GSI_NOTHR_LIBS=""
    LCMAPS_GLOBUS_GSI_THR_LIBS=""

    AC_ARG_WITH(gsi_mode,
        [  --with-gsi-mode              create lcmaps flavour that provides a GSI interface (default)],
        [],
        with_gsi_mode="yes")

    if test x$with_gsi_mode = xyes ; then
        dnl variable to be used in AM_CONDITIONAL
        lcmaps_gsi_mode=true

        AC_DEFINE(LCMAPS_GSI_MODE, 1, "If defined provide the GSI interface to LCMAPS")

        if test -n "$GLOBUS_NOTHR_CFLAGS" ; then
            LCMAPS_GLOBUS_NOTHR_CFLAGS="$GLOBUS_NOTHR_CFLAGS"
            LCMAPS_GLOBUS_THR_CFLAGS="$GLOBUS_THR_CFLAGS"
        fi
        if test -n "$GLOBUS_GSS_NOTHR_LIBS" ; then
            LCMAPS_GLOBUS_GSS_NOTHR_LIBS="$GLOBUS_GSS_NOTHR_LIBS"
            LCMAPS_GLOBUS_GSS_THR_LIBS="$GLOBUS_GSS_THR_LIBS"
        fi
        if test -n "$GLOBUS_GSI_NOTHR_LIBS" ; then
            LCMAPS_GLOBUS_GSI_NOTHR_LIBS="$GLOBUS_GSI_NOTHR_LIBS"
            LCMAPS_GLOBUS_GSI_THR_LIBS="$GLOBUS_GSI_THR_LIBS"
        fi
        LCMAPS_FLAVOUR=""

        AC_MSG_RESULT([Creating GSI flavour of LCMAPS])
    else
        dnl variable to be used in AM_CONDITIONAL
        lcmaps_gsi_mode=false

        PACKAGE="$PACKAGE-without-gsi"
        AC_MSG_RESULT([Setting package name to $PACKAGE])
        AC_SUBST(PACKAGE)

        PACKAGE_TARNAME="$PACKAGE_TARNAME-without-gsi"
        AC_MSG_RESULT([setting package tarname $PACKAGE_TARNAME])
        AC_SUBST(PACKAGE_TARNAME)
        LCMAPS_FLAVOUR="_without_gsi"

        AC_MSG_RESULT([Creating GSI-free flavour of LCMAPS])
    fi

    AM_CONDITIONAL(LCMAPS_GSI_MODE, test x$lcmaps_gsi_mode = xtrue)

    AC_SUBST(LCMAPS_FLAVOUR)
    AC_SUBST(LCMAPS_GLOBUS_NOTHR_CFLAGS)
    AC_SUBST(LCMAPS_GLOBUS_THR_CFLAGS)
    AC_SUBST(LCMAPS_GLOBUS_GSS_NOTHR_LIBS)
    AC_SUBST(LCMAPS_GLOBUS_GSS_THR_LIBS)
    AC_SUBST(LCMAPS_GLOBUS_GSI_NOTHR_LIBS)
    AC_SUBST(LCMAPS_GLOBUS_GSI_THR_LIBS)
])

