dnl 
dnl Set Xalan Properties
dnl
AC_DEFUN([AC_XALAN],
[
	# Set Xalan Properties
	AC_ARG_WITH(xalanc_prefix,
		[  --with-xalanc-prefix=PFX     prefix where XALAN-C is installed. (/opt/xalan-c)],
		[],
	        with_xalanc_prefix=${XALANCROOT:-/opt/xalan-c})
	        
	XALANCROOT="$with_xalanc_prefix"
	XALANC_CFLAGS="-I$with_xalanc_prefix/include"
	ac_xalanc_ldlib="-L$with_xalanc_prefix/lib"
	XALANC_LIBS="$ac_xalanc_ldlib -lxalan-c -lxalanMsg"
	    
	AC_SUBST(XALANCROOT)
	AC_SUBST(XALANC_CFLAGS)
	AC_SUBST(XALANC_LIBS)
])
