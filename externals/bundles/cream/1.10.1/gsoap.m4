dnl
dnl Define GSOAP
dnl
AC_DEFUN(AC_GSOAP,
[
	dnl
	dnl GSOAP Location
	dnl
	AC_ARG_WITH(gsoap-location,
	[  --with-gsoap-location=PFX     prefix where GSOAP is installed. (/opt/gsoap)],
	[],
        with_gsoap_location=${GSOAP_LOCATION:-/opt/gsoap})

	AC_MSG_RESULT([checking for gsoap... ])

	if test -n "$with_gsoap_location" ; then
		GSOAP_LOCATION="$with_gsoap_location"
		GSOAP_CFLAGS="-I$with_gsoap_location/include -I$with_gsoap_location -I$with_gsoap_location/extras"
		ac_gsoap_ldlib="-L$with_gsoap_location/lib"
		GSOAP_LIBS="$ac_gsoap_ldlib -lgsoap"
    else
		GSOAP_LOCATION=""
		GSOAP_CFLAGS=""
		GSOAP_LIBS=""
    fi

	dnl
	dnl GSOAP WSDL2H Location
	dnl
	AC_ARG_WITH(gsoap-wsdl2h-location,
	[  --with-gsoap-wsdl2h-location=PFX     prefix where GSOAP wsdl2h is installed. (/opt/gsoap)],
	[],
        with_gsoap_wsdl2h_location=${GSOAP_WSDL2H_LOCATION:-$GSOAP_LOCATION})

	AC_MSG_RESULT([checking for gsoap wsdl2h... ])

	if test -n "$with_gsoap_wsdl2h_location" ; then
		GSOAP_WSDL2H_LOCATION="$with_gsoap_wsdl2h_location"
    else
		GSOAP_WSDL2H_LOCATION="$GSOAP_LOCATION"
    fi

	dnl
	dnl GSOAP Version
	dnl
	AC_ARG_WITH(gsoap-version,
	[  --with-gsoap-version=PFX     GSOAP version (2.3.8)],
	[],
        with_gsoap_version=${GSOAP_VERSION:-2.3.8})

	AC_MSG_RESULT([checking for gsoap version... ])
	
	if test -n "$with_gsoap_version" ; then
		GSOAP_VERSION="$with_gsoap_version"
    else
		GSOAP_VERSION=""
    fi
    
    dnl
	dnl GSOAP WSDL2H Version
	dnl
	AC_ARG_WITH(gsoap-wsdl2h-version,
	[  --with-gsoap-wsdl2h-version=PFX     GSOAP WSDL2h version (2.3.8)],
	[],
        with_gsoap_wsdl2h_version=${GSOAP_WSDL2H_VERSIONS:-$GSOAP_VERSION})

	AC_MSG_RESULT([checking for gsoap WSDL2H version... ])
	
	if test -n "$with_gsoap_wsdl2h_version" ; then
		GSOAP_WSDL2H_VERSION="$with_gsoap_wsdl2h_version"
    else
		GSOAP_WSDL2H_VERSION="$GSOAP_VERSION"
    fi
    
    dnl
	dnl Set GSOAP Version Number as a compiler Definition
	dnl
	if test -n "$GSOAP_VERSION" ; then
		EXPR='foreach $n(split(/\./,"'$GSOAP_VERSION'")){if(int($n)>99){$n=99;}printf "%02d",int($n);} print "\n";'
		GSOAP_VERSION_NUM=`perl -e "$EXPR"`
	else
		GSOAP_VERSION_NUM=000000
	fi;		
	GSOAP_CFLAGS="$GSOAP_CFLAGS -D_GSOAP_VERSION=0x$GSOAP_VERSION_NUM"

	dnl
	dnl Set GSOAP WSDL2H Version Number as a compiler Definition
	dnl
	if test -n "$GSOAP_WSDL2H_VERSION" ; then
		EXPR='foreach $n(split(/\./,"'$GSOAP_WSDL2H_VERSION'")){if(int($n)>99){$n=99;}printf "%02d",int($n);} print "\n";'
		GSOAP_WSDL2H_VERSION_NUM=`perl -e "$EXPR"`
	else
		GSOAP_WSDL2H_VERSION_NUM=000000
	fi;		
	GSOAP_CFLAGS="$GSOAP_CFLAGS -D_GSOAP_WSDL2H_VERSION=0x$GSOAP_WSDL2H_VERSION_NUM"

    AC_MSG_RESULT([GSOAP_LOCATION set to $GSOAP_LOCATION])
	AC_MSG_RESULT([GSOAP_WSDL2H_LOCATION set to $GSOAP_WSDL2H_LOCATION])
    AC_MSG_RESULT([GSOAP_CFLAGS set to $GSOAP_CFLAGS])
    AC_MSG_RESULT([GSOAP_LIBS set to $GSOAP_LIBS])
    AC_MSG_RESULT([GSOAP_VERSION set to $GSOAP_VERSION])
    AC_MSG_RESULT([GSOAP_WSDL2H_VERSION set to $GSOAP_WSDL2H_VERSION])

	AC_SUBST(GSOAP_LOCATION)
	AC_SUBST(GSOAP_WSDL2H_LOCATION)
	AC_SUBST(GSOAP_CFLAGS)
	AC_SUBST(GSOAP_LIBS)
    AC_SUBST(GSOAP_VERSION)
    AC_SUBST(GSOAP_VERSION_NUM)
	AC_SUBST(GSOAP_WSDL2H_VERSION)
    AC_SUBST(GSOAP_WSDL2H_VERSION_NUM)
    
    dnl
	dnl Test GSOAP Version
	dnl
	if test [ "$GSOAP_VERSION_NUM" -ge "020300" -a "$GSOAP_VERSION_NUM" -lt "020600"] ; then
    	AC_MSG_RESULT([GSOAP_VERSION is 2.3])
		GSOAP_MAIN_VERSION=`expr substr "$GSOAP_VERSION" 1 3`
		SOAPCPP2="$GSOAP_LOCATION/bin/soapcpp2"
		SOAPCPP2_FLAGS="-n"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAP_2_3"
	elif test [ "$GSOAP_VERSION_NUM" -ge "020600"] ; then
    	AC_MSG_RESULT([GSOAP_VERSION is 2.6 or newer])
		GSOAP_MAIN_VERSION=`expr substr "$GSOAP_VERSION" 1 3`
		SOAPCPP2="$GSOAP_LOCATION/bin/soapcpp2"
		SOAPCPP2_FLAGS="-n -w"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAP_2_6"
	else
    	AC_MSG_RESULT([Unsupported GSOAP_VERSION])
		GSOAP_MAIN_VERSION=""
		SOAPCPP2=""
		SOAPCPP2_FLAGS=""
		GSOAP_CFLAGS=""
	fi
	
	dnl
	dnl Test GSOAP WSDL2H Version
	dnl
	if test [ "$GSOAP_WSDL2H_VERSION_NUM" -ge "020300" -a "$GSOAP_WSDL2H_VERSION_NUM" -lt "020600"] ; then
		AC_MSG_RESULT([GSOAP_WSDL2H_VERSION is 2.3])
	    	
	    dnl
		dnl Check Xerces Location
		dnl
		AC_MSG_RESULT([checking for xerces (neeeded by GSOAP 2.3)... ])
		
		AC_ARG_WITH(xerces-location,
				[  --with-xerces-location=PFX     prefix where Xerces is installed. (/usr/share/java/xerces)],
				[],
		    	    with_xerces_location=${XERCES_LOCATION:-/usr/share/java/xerces})
		
		if test -n "$with-xerces-location" ; then
			XERCES_LOCATION="$with_xerces_location"
		else
			XERCES_LOCATION=""
		fi
		AC_MSG_RESULT([XERCES_LOCATION set to $XERCES_LOCATION])
		
		AC_SUBST(XERCES_LOCATION)

		GSOAP_WSDL2H_MAIN_VERSION=`expr substr "$GSOAP_WSDL2H_VERSION" 1 3`
		WSDL2H="java -classpath $GSOAP_WSDL2H_LOCATION/share/wsdlcpp.jar:$XERCES_LOCATION/xmlParserAPIs.jar:$XERCES_LOCATION/xercesImpl.jar wsdlcpp"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAPWSDL2H_2_3"
	elif test [ "$GSOAP_WSDL2H_VERSION_NUM" -ge "020600"] ; then
    	AC_MSG_RESULT([GSOAP_WSDL2H_VERSION is 2.6 or newer])
		GSOAP_WSDL2H_MAIN_VERSION=`expr substr "$GSOAP_WSDL2H_VERSION" 1 3`
		WSDL2H="$GSOAP_WSDL2H_LOCATION/bin/wsdl2h"
		GSOAP_CFLAGS="$GSOAP_CFLAGS -DUSEGSOAPWSDL2H_2_6"
	else
    	AC_MSG_RESULT([Unsupported GSOAP_WSDL2H_VERSION])
		GSOAP_WSDL2H_MAIN_VERSION=""
		WSDL2H=""
	fi

	dnl
	dnl Obsolete !? Should be removed !?
	dnl
    AM_CONDITIONAL(USEGSOAP_2_3, 	   [test x$GSOAP_MAIN_VERSION = x2.3])
	AM_CONDITIONAL(USEGSOAP_2_6, 	   [test x$GSOAP_MAIN_VERSION = x2.6])
	AM_CONDITIONAL(USEGSOAPWSDL2H_2_3, [test x$GSOAP_WSDL2H_MAIN_VERSION = x2.3])
	AM_CONDITIONAL(USEGSOAPWSDL2H_2_6, [test x$GSOAP_WSDL2H_MAIN_VERSION = x2.6])

	AC_SUBST(GSOAP_CFLAGS)
	AC_SUBST(WSDL2H)
	AC_SUBST(SOAPCPP2)
	AC_SUBST(SOAPCPP2_FLAGS)
	AC_SUBST(GSOAP_MAIN_VERSION)
	AC_SUBST(GSOAP_WSDL2H_MAIN_VERSION)
])

