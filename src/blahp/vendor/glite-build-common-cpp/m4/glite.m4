dnl Usage:
dnl AC_GLITE
dnl - GLITE_LOCATION
dnl - GLITE_CFLAGS
dnl - GLITE_LDFLAGS
dnl - DISTTAR

AC_DEFUN([AC_GLITE],
[
    AC_ARG_WITH(glite_location,
        [  --with-glite-location=PFX     prefix where GLITE is installed. (/opt/glite)],
        [],
        with_glite_location=/opt/glite)

    if test -n "with_glite_location" ; then
    	GLITE_LOCATION="$with_glite_location"
	GLITE_CFLAGS="-I$GLITE_LOCATION/include"
    else
	GLITE_LOCATION=""
	GLITE_CFLAGS=""
    fi

    AC_MSG_RESULT([GLITE_LOCATION set to $GLITE_LOCATION])

    AC_SUBST(GLITE_LOCATION)
    AC_SUBST(GLITE_CFLAGS)

    if test "x$host_cpu" = "xx86_64"; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib64"
    else
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi

    AC_SUBST(GLITE_LDFLAGS)

    AC_ARG_WITH(dist_location,
        [  --with-dist-location=PFX     prefix where DIST location is. (pwd)],
        [],
        with_dist_location=$WORKDIR/../dist)

    DISTTAR=$with_dist_location

    AC_SUBST(DISTTAR)

    if test "x$host_cpu" = "xx86_64"; then
        AC_SUBST([libdir], ['${exec_prefix}/lib64'])
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    AC_SUBST([mandir], ['${prefix}/share/man'])
])

AC_DEFUN([GLITE_CHECK_LIBDIR],
[
    if test "x$host_cpu" = "xx86_64"; then
        AC_SUBST([libdir], ['${exec_prefix}/lib64'])
    fi

    # added to handle Debian 4/5
    if test -h "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    # added to handle platforms that don't use lib64 at all
    if ! test -e "/usr/lib64" ; then
        AC_SUBST([libdir], ['${exec_prefix}/lib'])
    fi

    AC_SUBST([mandir], ['${prefix}/share/man'])

    AC_ARG_VAR(PVER, Override the version string)
    AC_SUBST([PVER], [${PVER:-$VERSION}])

])

AC_DEFUN([GLITE_CHECK_INITDIR],
[
    if test -d /etc/rc.d -a -d /etc/rc.d/init.d ; then
        AC_SUBST([initdir], [rc.d/init.d])
    else
        AC_SUBST([initdir], [init.d])
    fi
])


# GLITE_BASE
# check that the gLite location directory is present
# if so, set GLITE_LOCATION, GLITE_CPPFLAGS and GLITE_LDFLAGS
# if the host is an x86_64 reset libdir
# reset mandir to comply with FHS
AC_DEFUN([GLITE_BASE],
[AC_ARG_WITH(
    [glite_location],
    [AS_HELP_STRING(
       [--with-glite-location=PFX],
       [prefix where gLite is installed @<:@default=/opt/glite@:>@]
    )],
    [],
    [with_glite_location=/opt/glite]
)
AC_MSG_CHECKING([for gLite location])
if test -d "$with_glite_location"; then
    GLITE_LOCATION="$with_glite_location"
    GLITE_CPPFLAGS="-I $GLITE_LOCATION/include"
    if test "x$host_cpu" = "xx86_64"; then
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib64"
    else
        GLITE_LDFLAGS="-L$GLITE_LOCATION/lib"
    fi
    AC_MSG_RESULT([$with_glite_location])
else
    AC_MSG_ERROR([$with_glite_location: no such directory])
fi

AC_SUBST([GLITE_LOCATION])
AC_SUBST([GLITE_CPPFLAGS])
AC_SUBST([GLITE_LDFLAGS])

if test "x$host_cpu" = "xx86_64"; then
    AC_SUBST([libdir], ['${exec_prefix}/lib64'])
fi

# added to handle Debian 4/5
if test -h "/usr/lib64" ; then
    AC_SUBST([libdir], ['${exec_prefix}/lib'])
fi

# added to handle platforms that don't use lib64 at all
if ! test -e "/usr/lib64" ; then
    AC_SUBST([libdir], ['${exec_prefix}/lib'])
fi

AC_SUBST([mandir], ['${prefix}/share/man'])
])
