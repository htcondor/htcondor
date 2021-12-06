dnl Usage:
dnl AC_SWIG(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for swig, and defines
dnl - SWIG_VERSION (siwg version)
dnl - SWIG_BIN_PATH (swig binary path)
dnl prerequisites:

AC_DEFUN([AC_SWIG],
[
    AC_ARG_WITH(swig_prefix,
	[  --with-swig-prefix=PFX       prefix where 'swig' is installed.],
	[], 
	with_swig_prefix=${SWIG_INSTALL_PATH:-/usr})

    if test -n "$with_swig_prefix"; then
        SWIG_BIN_PATH="$with_swig_prefix/bin"
    else
        SWIG_BIN_PATH=""
    fi

    PSWIG=""

    result=no

    AC_PATH_PROG(PSWIG,swig,no,$SWIG_BIN_PATH)
    if test "$PSWIG" != "no" ; then
        $PSWIG -version &> swig.ver

        changequote(<<, >>)
        swig_major_ver=`cat swig.ver | sed 's/.* \([0-9]*\)\.[0-9]*\.[0-9]*.*/\1/p; d'`
        swig_minor_ver=`cat swig.ver | sed 's/.* [0-9]*\.\([0-9]*\)\.[0-9]*.*/\1/p; d'`
        swig_micro_ver=`cat swig.ver | sed 's/.* [0-9]*\.[0-9]*\.\([0-9]*\).*/\1/p; d'`
        changequote([, ])

        if test "$swig_major_ver" = "" ; then
            changequote(<<, >>)
            swig_major_ver=`cat swig.ver | sed 's/.* \([0-9]*\)\.[0-9]*.*/\1/p; d'`
            swig_minor_ver=`cat swig.ver | sed 's/.* [0-9]*\.\([0-9]*\).*/\1/p; d'`
            changequote([, ])

            SWIG_VERSION="$swig_major_ver.$swig_minor_ver"
        else
            SWIG_VERSION="$swig_major_ver.$swig_minor_ver.$swig_micro_ver"
        fi

        rm -f swig.ver

        changequote(<<, >>)
        ac_req_major=`echo "$1" | sed 's/\([0-9]\+\)\.[0-9]\+\.\?[0-9]*/\1/p; d'
`
        ac_req_minor=`echo "$1" | sed 's/[0-9]\+\.\([0-9]\+\)\.\?[0-9]*/\1/p; d'
`
        ac_req_micro=`echo "$1" | sed 's/[0-9]\+\.[0-9]\+\.\?\([0-9]*\)/\1/p; d'
`
        changequote([, ])

        if test $swig_minor_ver -ne $ac_req_minor; then
            result=no
        else
            if test $swig_major_ver -eq $ac_req_major -a $swig_minor_ver -eq $ac_req_minor -a $swig_micro_ver -ge $ac_req_micro; then
                result=yes
            fi
        fi
    else
	ac_cv_swig_valid=no
	AC_MSG_RESULT([Could not find swig tool!])
    fi

    ac_cv_swig_valid=$result

    if test "x$ac_cv_swig_valid" = "xno" ; then
        AC_MSG_RESULT([swig status: **** suitable version NOT FOUND])
    else
        AC_MSG_RESULT([swig status: **** suitable version FOUND])
    fi                                                                          

    AC_MSG_RESULT([swig *required* version: $1])
    AC_MSG_RESULT([swig *found* version: $SWIG_VERSION])

    if test "x$ac_cv_swig_valid" = "xyes" ; then
        SWIG_INSTALL_PATH=$with_swig_prefix
        ifelse([$2], , :, [$2])
    else
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(SWIG_INSTALL_PATH)
    AC_SUBST(SWIG_VERSION)
    AC_SUBST(SWIG_BIN_PATH)
])

