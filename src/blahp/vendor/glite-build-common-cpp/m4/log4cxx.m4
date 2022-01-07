dnl AM_PATH_LOG4CXX([ ])
dnl define LOG4CXX_CFLAGS and LOG4CXX_LIBS
dnl
AC_DEFUN([AM_PATH_LOG4CXX],
[
	# Set Log4Cxx Properties
	AC_ARG_WITH(log4cxx_prefix,
		[  --with-log4cxx-prefix=PFX     prefix where Log4Cxx is installed. (/opt/log4cxx)],
		[],
	        with_log4cxx_prefix=${LO4CXX_PATH:-/opt/log4cxx})
	        
	LO4CXX_PATH="$with_log4cxx_prefix"
	LOG4CXX_CFLAGS="-I$with_log4cxx_prefix/include"
	ac_log4cxx_ldlib="-L$with_log4cxx_prefix/lib"
	LOG4CXX_LIBS="$ac_log4cxx_ldlib -llog4cxx"
	    
	AC_SUBST(LO4CXX_PATH)
	AC_SUBST(LOG4CXX_CFLAGS)
	AC_SUBST(LOG4CXX_LIBS)
])
