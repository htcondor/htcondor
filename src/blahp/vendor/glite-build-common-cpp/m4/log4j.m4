dnl Usage:
dnl AC_LOG4J(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - LOG4J_INSTALL_PATH (compiler flags)
dnl prerequisites:

AC_DEFUN([AC_LOG4J],
[
    AC_ARG_WITH(log4j_prefix, 
	[  --with-log4j-prefix=PFX   prefix where 'log4j' is installed.],
	[], 
	with_log4j_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for LOG4J installation at ${with_log4j_prefix}])

    LOG4J_INSTALL_PATH=$with_log4j_prefix

    ac_log4j=yes

    if test x$ac_log4j = xyes; then
	ifelse([$2], , :, [$2])
    else
	LOG4J_INSTALL_PATH=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(LOG4J_INSTALL_PATH)
])

