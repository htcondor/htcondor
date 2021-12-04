dnl Usage:
dnl GLITE_CHECK_JOBID
dnl - GLITE_JOBID_CFLAGS
dnl - GLITE_JOBID_LIBS

AC_DEFUN([GLITE_CHECK_JOBID],
[
    AC_MSG_CHECKING([for org.glite.jobid])

    AC_LANG_PUSH([C++])
    save_CPPFLAGS=$CPPFLAGS
    save_LIBS=$LIBS

    AC_ARG_WITH(glite-jobid-prefix,
        [  --with-glite-jobid-prefix=PFX     prefix where jobid is installed. (/usr)],
        [],
        with_glite_jobid_prefix=${GLITE_LOCATION:-/usr})

    if test "x$host_cpu" = "xx86_64"; then
        ac_glite_jobid_lib="-L$with_glite_jobid_prefix/lib64"
    else
        ac_glite_jobid_lib="-L$with_glite_jobid_prefix/lib"
    fi
    if test -h "/usr/lib64" ; then
        ac_glite_jobid_lib="-L$with_glite_jobid_prefix/lib"
    fi
    if ! test -e "/usr/lib64" ; then
        ac_glite_jobid_lib="-L$with_glite_jobid_prefix/lib"
    fi
    GLITE_JOBID_CFLAGS="-I$with_glite_jobid_prefix/include -I$with_glite_jobid_prefix/include/glite"
    GLITE_JOBID_LIBS="$ac_glite_jobid_lib -lglite_jobid"

    CPPFLAGS="$CPPFLAGS $GLITE_JOBID_CFLAGS"
    LIBS="$LIBS $GLITE_JOBID_LIBS"

    AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([#include "glite/jobid/JobId.h"],
                         [glite::jobid::JobId id;])],
        [have_glite_jobid=yes],
        [have_glite_jobid=no]
    )

    LIBS=$save_LIBS
    CPPFLAGS=$save_CPPFLAGS
    AC_LANG_POP([C++])

    if test x"$have_glite_jobid" = xyes; then
        AC_SUBST(GLITE_JOBID_CFLAGS)
        AC_SUBST(GLITE_JOBID_LIBS)
        m4_default([$2], [AC_MSG_RESULT([yes])])
    else
        m4_default([$3], [AC_MSG_ERROR([cannot find org.glite.jobid])])
    fi

])

