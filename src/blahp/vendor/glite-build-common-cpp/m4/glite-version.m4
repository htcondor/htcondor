dnl Usage:
dnl AC_GLITE_VERSION
dnl - VERSION
dnl - INTERFACE_VERSION

AC_DEFUN([AC_GLITE_VERSION],
[	# Define Version
	AC_ARG_WITH(version,
			[  --with-version=VRS    module version],
			[],with_version=${VERSION:-0.0.0})
	if test -n "$with_version" ; then
			VERSION="$with_version"
			AC_SUBST(VERSION)
	fi
	AC_MSG_RESULT([VERSION is $VERSION])
	
	# Define Interface Version
	AC_ARG_WITH(interface_version,
			[  --with-interface-version=IFCV    interface version],
			[],with_interface_version=${INTERFACE_VERSION:-0.0.0})
	if test -n "$with_interface_version" ; then
	       	INTERFACE_VERSION="$with_interface_version"
	       	AC_SUBST(INTERFACE_VERSION)
	fi
	AC_MSG_RESULT([INTERFACE_VERSION is $INTERFACE_VERSION])

	dnl Split the version code
	INTERFACE_MAJOR=`echo $INTERFACE_VERSION | cut -d. -f1`
	INTERFACE_MINOR=`echo $INTERFACE_VERSION | cut -d. -f2`
	INTERFACE_PATCHLEVEL=`echo $INTERFACE_VERSION | cut -d. -f3`

	dnl Calculate the libtool version fields
	INTERFACE_LIBTOOL_CURRENT=`expr $INTERFACE_MAJOR + $INTERFACE_MINOR`
	INTERFACE_LIBTOOL_REVISION=$INTERFACE_PATCHLEVEL
	INTERFACE_LIBTOOL_AGE=$INTERFACE_MINOR

	dnl Export these variables to Automake
	AC_SUBST([INTERFACE_LIBTOOL_CURRENT])
	AC_SUBST([INTERFACE_LIBTOOL_REVISION])
	AC_SUBST([INTERFACE_LIBTOOL_AGE])
])
	
