dnl Usage:
dnl AC_JGLOBUS(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - JGLOBUS_INSTALL_PATH (compiler flags)
dnl prerequisites:

AC_DEFUN([AC_JGLOBUS],
[
    AC_ARG_WITH(jglobus_prefix, 
	[  --with-jglobus-prefix=PFX   prefix where 'jglobus' is installed.],
	[], 
	with_jglobus_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for JGLOBUS installation at ${with_jglobus_prefix}])

    JGLOBUS_INSTALL_PATH=$with_jglobus_prefix

    ac_jglobus=yes

    if test x$ac_jglobus = xyes; then
	ifelse([$2], , :, [$2])
    else
	JGLOBUS_INSTALL_PATH=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(JGLOBUS_INSTALL_PATH)
])

