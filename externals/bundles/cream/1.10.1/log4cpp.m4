dnl AM_PATH_LOG4CPP([ ])
dnl define LOG4CPP_CFLAGS and LOG4CPP_LIBS
dnl
AC_DEFUN(AM_PATH_LOG4CPP,
[
	# Set Log4Cpp Properties
	AC_ARG_WITH(log4cpp_prefix,
		[  --with-log4cpp-prefix=PFX     prefix where Log4Cpp is installed. (/opt/log4cpp)],
		[],
	        with_log4cpp_prefix=${LO4CPP_PATH:-/opt/log4cpp})
	        
	LO4CPP_PATH="$with_log4cpp_prefix"
	LOG4CPP_CFLAGS="-I$with_log4cpp_prefix/include"
	ac_log4cpp_ldlib="-L$with_log4cpp_prefix/lib"
	LOG4CPP_LIBS="$ac_log4cpp_ldlib -llog4cpp"
	    
	AC_SUBST(LO4CPP_PATH)
	AC_SUBST(LOG4CPP_CFLAGS)
	AC_SUBST(LOG4CPP_LIBS)
])