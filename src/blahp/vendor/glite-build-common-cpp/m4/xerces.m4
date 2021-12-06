dnl 
dnl Set Xerces Properties
dnl
AC_DEFUN([AC_XERCES],
[
	# Set Xerces Properties
	AC_ARG_WITH(xercesc_prefix,
		[  --with-xercesc-prefix=PFX     prefix where XERCES-C is installed. (/opt/xerces-c)],
		[],
        	with_xercesc_prefix=${XERCESCROOT:-/opt/xerces-c})
        
	XERCESCROOT="$with_xercesc_prefix"
	XERCESC_CFLAGS="-I$with_xercesc_prefix/include"
	ac_xercesc_ldlib="-L$with_xercesc_prefix/lib"
	XERCESC_LIBS="$ac_xercesc_ldlib -lxerces-c"
    
	AC_SUBST(XERCESCROOT)
	AC_SUBST(XERCESC_CFLAGS)
	AC_SUBST(XERCESC_LIBS)
])
