dnl Usage:
dnl AC_JCLASSAD(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - JCLASSAD_INSTALL_PATH (compiler flags)
dnl prerequisites:

AC_DEFUN([AC_JCLASSAD],
[
    AC_ARG_WITH(jclassad_prefix, 
	[  --with-jclassad-prefix=PFX   prefix where 'jclassad' is installed.],
	[], 
	with_jclassad_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for JCLASSAD installation at ${with_jclassad_prefix}])

    JCLASSAD_INSTALL_PATH=$with_jclassad_prefix

    ac_jclassad=yes

    if test x$ac_jclassad = xyes; then
	ifelse([$2], , :, [$2])
    else
	JCLASSAD_INSTALL_PATH=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(JCLASSAD_INSTALL_PATH)
])

