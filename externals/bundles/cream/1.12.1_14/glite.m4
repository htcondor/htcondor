dnl Usage:
dnl AC_GLITE
dnl - GLITE_LOCATION
dnl - GLITE_CFLAGS
dnl - DISTTAR

AC_DEFUN(AC_GLITE,
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

    AC_ARG_WITH(dist_location,
        [  --with-dist-location=PFX     prefix where DIST location is. (pwd)],
        [],
        with_dist_location=$WORKDIR/../dist)

    DISTTAR=$with_dist_location

    AC_SUBST(DISTTAR)

])

