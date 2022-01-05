AC_DEFUN([GLITE_CHECK_WMSUTILS_CLASSADS],
[AC_MSG_CHECKING([for org.glite.wmsutils.classads])
AC_LANG_PUSH([C++])
save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $GLITE_CPPFLAGS $CLASSAD_CPPFLAGS"
save_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS $GLITE_LDFLAGS $CLASSAD_LDFLAGS"
save_LIBS=$LIBS
LIBS="-lglite_wmsutils_classads $CLASSAD_LIBS $LIBS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
     [#include "glite/wmsutils/classads/classad_utils.h"],
     [glite::wmsutils::classads::parse_classad("");]
  )],
  [have_wmsutils_classads=yes],
  [have_wmsutils_classads=no]
)
LIBS=$save_LIBS
LDFLAGS=$save_LDFLAGS
CPPFLAGS=$save_CPPFLAGS
AC_LANG_POP([C++])
if test x"$have_wmsutils_classads" = xyes; then
  AC_SUBST([GLITE_WMSUTILS_CLASSADS_LIBS], [-lglite_wmsutils_classads])
  m4_default([$2], [AC_MSG_RESULT([yes])])
else
  m4_default([$3], [AC_MSG_ERROR([cannot find org.glite.wmsutils.classads])])
fi
]) # GLITE_CHECK_WMSUTILS_CLASSADS

AC_DEFUN([GLITE_CHECK_WMSUTILS_EXCEPTION],
[AC_MSG_CHECKING([for org.glite.wmsutils.exception])
AC_LANG_PUSH([C++])
save_CPPFLAGS=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $GLITE_CPPFLAGS"
save_LDFLAGS=$LDFLAGS
LDFLAGS="$LDFLAGS $GLITE_LDFLAGS"
save_LIBS=$LIBS
LIBS="-lglite_wmsutils_exception $LIBS"
AC_LINK_IFELSE(
  [AC_LANG_PROGRAM(
     [#include "glite/wmsutils/exception/Exception.h"],
     [try {} catch(glite::wmsutils::exception::Exception const& e) {e.what();}]
  )],
  [have_wmsutils_exception=yes],
  [have_wmsutils_exception=no]
)
LIBS=$save_LIBS
LDFLAGS=$save_LDFLAGS
CPPFLAGS=$save_CPPFLAGS
AC_LANG_POP([C++])
if test x"$have_wmsutils_exception" = xyes; then
  AC_SUBST([GLITE_WMSUTILS_EXCEPTION_LIBS], [-lglite_wmsutils_exception])
  m4_default([$2], [AC_MSG_RESULT([yes])])
else
  m4_default([$3], [AC_MSG_ERROR([cannot find org.glite.wmsutils.exception])])
fi
]) # GLITE_CHECK_WMSUTILS_EXCEPTION

dnl Usage:
dnl AC_GLITE_WMSUTILS
dnl - GLITE_WMSUTILS_EXCEPTION_LIBS
dnl -
dnl - GLITE_WMSUTILS_CJOBID_LIBS
dnl - GLITE_WMSUTILS_JOBID_LIBS
dnl -
dnl - GLITE_WMSUTILS_TLS_SOCKET_LIBS
dnl - GLITE_WMSUTILS_TLS_GSI_SOCKET_LIBS
dnl - GLITE_WMSUTILS_TLS_GSISOCKET_LIBS
dnl -
dnl - GLITE_WMSUTILS_CLASSADS_LIBS

AC_DEFUN([AC_GLITE_WMSUTILS],
[
    ac_glite_wmsutils_prefix=$GLITE_LOCATION
    ac_glite_wmsutils_lib="$GLITE_LDFLAGS"

    have_exception_pkconfig=no
    PKG_CHECK_MODULES(GLITE_WMSUTILS_EXCEPTION, jobman-exception, have_exception_pkgconfig=yes, have_exception_pkgconfig=no)
    have_classadutil_pkconfig=no
    PKG_CHECK_MODULES(GLITE_WMSUTILS_CLASSADS, classad-utils, have_classadutil_pkconfig=yes, have_classadutil_pkconfig=no)

    if test "x$have_exception_pkgconfig" = "xyes" -a "x$have_classadutil_pkconfig" == "xyes" ; then
        AC_MSG_RESULT([pkg-config detects WMSUTILS definitions])
        ifelse([$2], , :, [$2])
    elif test -n "$ac_glite_wmsutils_lib" ; then
	GLITE_WMSUTILS_EXCEPTION_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_exception"
        GLITE_WMSUTILS_CLASSADS_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_classads"

dnl     GLITE_WMSUTILS_CJOBID_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_cjobid"
dnl     GLITE_WMSUTILS_JOBID_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_jobid"
dnl     GLITE_WMSUTILS_TLS_SOCKET_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_tls_socket_pp"
dnl     GLITE_WMSUTILS_TLS_GSI_SOCKET_LIBS="$ac_glite_wmsutils_lib -lglite_wmsutils_tls_gsisocket_pp"
dnl     GLITE_WMSUTILS_TLS_GSISOCKET_LIBS="$GLITE_WMSUTILS_TLS_SOCKET_LIBS -lglite_wmsutils_tls_gsisocket_pp"
	
        ifelse([$2], , :, [$2])
    else
        GLITE_WMSUTILS_EXCEPTION_CFLAGS=""
        GLITE_WMSUTILS_EXCEPTION_LIBS=""
        GLITE_WMSUTILS_CLASSADS_CFLAGS=""
        GLITE_WMSUTILS_CLASSADS_LIBS=""

dnl     GLITE_WMSUTILS_CJOBID_LIBS=""
dnl     GLITE_WMSUTILS_JOBID_LIBS=""
dnl     GLITE_WMSUTILS_TLS_SOCKET_LIBS=""
dnl     GLITE_WMSUTILS_TLS_GSI_SOCKET_LIBS=""
dnl     GLITE_WMSUTILS_TLS_GSISOCKET_LIBS=""

	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMSUTILS_EXCEPTION_CFLAGS)
    AC_SUBST(GLITE_WMSUTILS_EXCEPTION_LIBS)
    AC_SUBST(GLITE_WMSUTILS_CLASSADS_CFLAGS)
    AC_SUBST(GLITE_WMSUTILS_CLASSADS_LIBS)

dnl AC_SUBST(GLITE_WMSUTILS_CJOBID_LIBS)
dnl AC_SUBST(GLITE_WMSUTILS_JOBID_LIBS)
dnl AC_SUBST(GLITE_WMSUTILS_TLS_SOCKET_LIBS)
dnl AC_SUBST(GLITE_WMSUTILS_TLS_GSI_SOCKET_LIBS)
dnl AC_SUBST(GLITE_WMSUTILS_TLS_GSISOCKET_LIBS)

])

