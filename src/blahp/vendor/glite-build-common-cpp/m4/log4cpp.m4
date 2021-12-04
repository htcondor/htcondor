dnl AM_PATH_LOG4CPP([ ])
dnl define LOG4CPP_CFLAGS and LOG4CPP_LIBS
dnl
AC_DEFUN([AM_PATH_LOG4CPP],
[
	# Set Log4Cpp Properties
	AC_ARG_WITH(log4cpp_prefix,
		[  --with-log4cpp-prefix=PFX     prefix where Log4Cpp is installed. (/usr)],
		[],
	        with_log4cpp_prefix=${LOG4CPP_PATH:-/usr})
	

        have_log4cpp=no
	PKG_CHECK_MODULES(LOG4CPP, log4cpp, have_log4cpp=yes, have_log4cpp=no)

	if test "x$have_log4cpp" = "xyes" ; then
		LOG4CPP_PATH=""
		ifelse([$2], , :, [$2])
	else
		if test $host_cpu = x86_64 -o "$host_cpu" = ia64 ; then
			ac_log4cpp_ldlib="-L$with_log4cpp_prefix/lib64"
        	else
			ac_log4cpp_ldlib="-L$with_log4cpp_prefix/lib"
        	fi

		LOG4CPP_PATH="$with_log4cpp_prefix"
		LOG4CPP_CFLAGS="-I$with_log4cpp_prefix/include"
		LOG4CPP_LIBS="$ac_log4cpp_ldlib -llog4cpp -lpthread"
	fi
    
	AC_SUBST(LOG4CPP_PATH)
	AC_SUBST(LOG4CPP_CFLAGS)
	AC_SUBST(LOG4CPP_LIBS)
])
