dnl Usage:
dnl AC_JAVA(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for java, and defines
dnl - JAVA_CFLAGS (compiler flags)
dnl - JAVA_LIBS (linker flags, stripping and path)
dnl - JAVA_VERSION (java version)
dnl - JAVA_BIN_PATH (java binary path)
dnl - JAVA_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_JAVA],
[
    AC_ARG_WITH(java_prefix,
	[  --with-java-prefix=PFX       prefix where 'java' is installed.],
	[], 
        with_java_prefix=${JAVA_INSTALL_PATH:-/usr/java/j2sdk1.4.1_01})

    if test -n "$with_java_prefix"; then
        JAVA_BIN_PATH="$with_java_prefix/bin"
    else
        JAVA_BIN_PATH=""
        JAVA_CFLAGS=""
        JAVA_LIBS=""
    fi

    PJAVA=""

    result=no

    AC_PATH_PROG(PJAVA,java,no,$JAVA_BIN_PATH)
    if test "$PJAVA" != "no" ; then
        $PJAVA -version &> java.ver

        changequote(<<, >>)
        java_major_ver=`cat java.ver | sed 's/.* "\([0-9]*\)\.[0-9]*\.[0-9]*.*/\1/p; d'`
        java_minor_ver=`cat java.ver | sed 's/.* "[0-9]*\.\([0-9]*\)\.[0-9]*.*/\1/p; d'`
        java_micro_ver=`cat java.ver | sed 's/.* "[0-9]*\.[0-9]*\.\([0-9]*\).*/\1/p; d'`
        changequote([, ])

        if test "$java_major_ver" = "" ; then
            changequote(<<, >>)
            java_major_ver=`cat java.ver | sed 's/.* " \([0-9]*\)\.[0-9]*.*/\1/p; d'`
            java_minor_ver=`cat java.ver | sed 's/.* " [0-9]*\.\([0-9]*\).*/\1/p; d'`
            changequote([, ])

            JAVA_VERSION="$java_major_ver.$java_minor_ver"
        else
            JAVA_VERSION="$java_major_ver.$java_minor_ver.$java_micro_ver"
        fi

        rm -f java.ver

	changequote(<<, >>)
        ac_req_major=`echo "$1" | sed 's/\([0-9]\+\)\.[0-9]\+\.\?[0-9]*/\1/p; d'`
        ac_req_minor=`echo "$1" | sed 's/[0-9]\+\.\([0-9]\+\)\.\?[0-9]*/\1/p; d'`
        ac_req_micro=`echo "$1" | sed 's/[0-9]\+\.[0-9]\+\.\?\([0-9]*\)/\1/p; d'`
        changequote([, ])

        if test $java_minor_ver -eq $ac_req_major; then
            result=no
        else
            if test $java_major_ver -eq $ac_req_major -a $java_minor_ver -eq $ac_req_minor -a $java_micro_ver -ge $ac_req_micro ; then
                result=yes
            fi
        fi
    else
	AC_MSG_RESULT([Could not find java tool!])
    fi

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS

    if test -n "$with_java_prefix" -a "x$result" = "xyes" ; then
        JAVA_CFLAGS="-I$with_java_prefix/include -I$with_java_prefix/include/linux"
        JAVA_LIBS="-L$with_java_prefix/lib"
    else
        JAVA_CFLAGS=""
        JAVA_LIBS=""
    fi

    CFLAGS="$JAVA_CFLAGS $CFLAGS"
    LIBS="$JAVA_LIBS $LIBS"

    AC_TRY_COMPILE([
         #include <jni.h>
       ],
       [ ],
       [ ac_cv_java_valid=yes ], [ ac_cv_java_valid=no ])

    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS

dnl    if test x$ac_cv_java_valid = xno ; then
dnl        AC_MSG_RESULT([java status: **** suitable version NOT FOUND])
dnl    else
dnl        AC_MSG_RESULT([java status: **** suitable version FOUND])
dnl    fi
                                                                                
dnl    AC_MSG_RESULT([java *required* version: $1])
    AC_MSG_RESULT([java *found* version: $JAVA_VERSION])

    if test "x$ac_cv_java_valid" = "xyes" ; then
        JAVA_INSTALL_PATH=$with_java_prefix
        ifelse([$2], , :, [$2])
    else
	JAVA_CFLAGS=""
	JAVA_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(JAVA_INSTALL_PATH)
    AC_SUBST(JAVA_VERSION)
    AC_SUBST(JAVA_BIN_PATH)
    AC_SUBST(JAVA_CFLAGS)
    AC_SUBST(JAVA_LIBS)
])

