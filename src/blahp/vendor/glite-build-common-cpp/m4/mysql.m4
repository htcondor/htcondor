dnl Usage:
dnl AC_MYSQL(MINIMUM-VERSION, MAXIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for mysql, and defines
dnl - MYSQL_CFLAGS (compiler flags)
dnl - MYSQL_LIBS (linker flags, stripping and path)
dnl prerequisites:

AC_DEFUN([mysql_version_between],
[
	l=0; for i in `echo $1 | tr . ' '`; do l=`expr $i + 1000 \* $l`; done
	m=0; for i in `echo $2 | tr . ' '`; do m=`expr $i + 1000 \* $m`; done
	r=0; for i in `echo $3 | tr . ' '`; do r=`expr $i + 1000 \* $r`; done

	if test $l -le $m -a $m -le $r; then [ $4 ]; else [ $5 ]; fi
])

AC_DEFUN([AC_MYSQL],
[
    AC_ARG_WITH(mysql_prefix, 
       [  --with-mysql-prefix=PFX      prefix where MySQL is installed.],
       , 
       with_mysql_prefix=${MYSQL_INSTALL_PATH:-/usr})

    if test -n "$with_mysql_prefix" -a "$with_mysql_prefix" != "/usr" ; then
        MYSQL_BIN_PATH=$with_mysql_prefix/bin
    else	
        MYSQL_BIN_PATH=/usr/bin
    fi
    
    AC_ARG_WITH(mysql_devel_prefix, 
       [  --with-mysql-devel-prefix=PFX      prefix where MySQL devel is installed.],
       , 
       with_mysql_devel_prefix=${MYSQL_INSTALL_PATH:-/usr})

    dnl
    dnl check the mysql version. if the major version is greater than 3
    dnl - then the MYSQL_LIBS macro is equal to -lmysqlclient -lz
    dnl - otherwise the MYSQL_LIBS macro is equal to -lmysqlclient
    dnl

    mysql_greater_than_3=no
    result=no
    ac_cv_mysql_valid=no

    AC_PATH_PROG(PMYSQL,mysql,no,$MYSQL_BIN_PATH)

    if test "$PMYSQL" != "no" ; then
        if test "x$HOSTTYPE" = "xx86_64"; then
            MYSQL_VERSION=`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$with_mysql_prefix/lib64/mysql/; $PMYSQL --version | cut -d' ' -f6 | tr -cd '0-9.'`
        else
	        MYSQL_VERSION=`export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$with_mysql_prefix/lib/mysql/; $PMYSQL --version | cut -d' ' -f6 | tr -cd '0-9.'`
	    fi

        mysql_version_between(3.0.0,$MYSQL_VERSION,3.999.999,
            mysql_greater_than_3=no,mysql_greater_than_3=yes)

	mysql_version_between($1,$MYSQL_VERSION,$2,result=yes,:)
    else
        AC_MSG_RESULT([Could not find mysql tool!])
    fi

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS

    if test -n "$with_mysql_devel_prefix" -a "$with_mysql_devel_prefix" != "/usr" -a "x$result" = "xyes" ; then
	MYSQL_CFLAGS="-I$with_mysql_devel_prefix/include -I$with_mysql_devel_prefix/include/mysql"
        if test "x$HOSTTYPE" = "xx86_64"; then
            MYSQL_LIBS="-L$with_mysql_devel_prefix/lib64 -L$with_mysql_devel_prefix/lib64/mysql"
        else
	        MYSQL_LIBS="-L$with_mysql_devel_prefix/lib -L$with_mysql_devel_prefix/lib/mysql"
        fi
    else
        MYSQL_CFLAGS=""
        MYSQL_LIBS=""
    fi

    if test -n "$with_mysql_prefix" -a "$with_mysql_prefix" != "/usr" -a "x$result" = "xyes" ; then
        if test "x$HOSTTYPE" = "xx86_64"; then
            MYSQL_LIBS="$MYSQL_LIBS -L$with_mysql_prefix/lib64 -L$with_mysql_prefix/lib64/mysql"
        else
            MYSQL_LIBS="$MYSQL_LIBS -L$with_mysql_prefix/lib -L$with_mysql_prefix/lib/mysql"
        fi
    fi

    if test "x$result" = "xyes" ; then
        if test "x$mysql_greater_than_3" = "xyes" ; then
            MYSQL_LIBS="$MYSQL_LIBS -lmysqlclient -lz"
        else
            MYSQL_LIBS="$MYSQL_LIBS -lmysqlclient"
        fi
        
        CFLAGS="$MYSQL_CFLAGS $CFLAGS"
        LIBS="$MYSQL_LIBS $LIBS"

        AC_TRY_COMPILE([
  	      #include <mysql.h>
          ],[ MYSQL_FIELD mf ],
          [ ac_cv_mysql_valid=yes ], [ ac_cv_mysql_valid=no ])

        CFLAGS=$ac_save_CFLAGS
        LIBS=$ac_save_LIBS
    fi

    AC_MSG_RESULT([mysql status:  $ac_cv_mysql_valid])
    if test "x$ac_cv_mysql_valid" = "xno" ; then
        AC_MSG_RESULT([mysql status: **** suitable version NOT FOUND])
    else
        AC_MSG_RESULT([mysql status: **** suitable version FOUND])
    fi

    AC_MSG_RESULT([mysql *required* version between $1 and $2])
    AC_MSG_RESULT([mysql *found* version: $MYSQL_VERSION])

    if test "x$ac_cv_mysql_valid" = "xyes" ; then
        MYSQL_INSTALL_PATH=$with_mysql_prefix
	ifelse([$3], , :, [$3])
    else
	    MYSQL_CFLAGS=""
	    MYSQL_LIBS=""
	ifelse([$4], , :, [$4])
    fi

    AC_SUBST(MYSQL_INSTALL_PATH)
    AC_SUBST(MYSQL_CFLAGS)
    AC_SUBST(MYSQL_LIBS)
])

