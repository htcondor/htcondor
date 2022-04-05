dnl Usage:
dnl AC_PYTHON(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for python, and defines
dnl - PYTHON_CFLAGS (compiler flags)
dnl - PYTHON_VERSION (python2.2 version)
dnl - PYTHON_BIN_PATH (python binary path)
dnl prerequisites:

AC_DEFUN([AC_PYTHON],
[
    AC_ARG_WITH(python_prefix, 
	[  --with-python-prefix=PFX     prefix where 'python' is installed.],
	[], 
        with_python_prefix=${PYTHON_INSTALL_PATH:-/usr})

    if test -n "$with_python_prefix"; then
        PYTHON_BIN_PATH="$with_python_prefix/bin"
    else
        PYTHON_BIN_PATH=""
    fi
   
    PPYTHON=""

    result=no

    AC_PATH_PROG(PPYTHON,python,no,$PYTHON_BIN_PATH)
    if test "$PPYTHON" != "no" ; then
        $PPYTHON -V &> python.ver
 
        changequote(<<, >>)
        python_major_ver=`cat python.ver | sed 's/.* \([0-9]*\)\.[0-9]*\.[0-9]*.*/\1/p; d'`
        python_minor_ver=`cat python.ver | sed 's/.* [0-9]*\.\([0-9]*\)\.[0-9]*.*/\1/p; d'`
        python_micro_ver=`cat python.ver | sed 's/.* [0-9]*\.[0-9]*\.\([0-9]*\).*/\1/p; d'`
        changequote([, ])

        if test "$python_major_ver" = "" ; then
            changequote(<<, >>)
            python_major_ver=`cat python.ver | sed 's/.* \([0-9]*\)\.[0-9]*.*/\1/p; d'`
            python_minor_ver=`cat python.ver | sed 's/.* [0-9]*\.\([0-9]*\).*/\1/p; d'`
            changequote([, ])

            PYTHON_VERSION=$python_major_ver.$python_minor_ver
        else
            PYTHON_VERSION=$python_major_ver.$python_minor_ver.$python_micro_ver
        fi

        PYTHON_MAJOR=$python_major_ver
        PYTHON_MINOR=$python_minor_ver

        rm -f python.ver

	changequote(<<, >>)
        ac_req_major=`echo "$1" | sed 's/\([0-9]\+\)\.[0-9]\+\.\?[0-9]*/\1/p; d'`
        ac_req_minor=`echo "$1" | sed 's/[0-9]\+\.\([0-9]\+\)\.\?[0-9]*/\1/p; d'`
        ac_req_micro=`echo "$1" | sed 's/[0-9]\+\.[0-9]\+\.\?\([0-9]*\)/\1/p; d'`
        changequote([, ])
       
        if test $python_major_ver -eq $ac_req_major -a $python_minor_ver -ge $ac_req_minor ; then
            result=yes
        else 
            if test $python_major_ver -eq $ac_req_major -a $python_minor_ver -ge $ac_req_minor -a $python_micro_ver -eq $ac_req_micro ; then
                result=yes
            fi
        fi
    else
	ac_cv_python_valid=no
	AC_MSG_RESULT([Could not find python tool!])
    fi

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS

    if test -n "$with_python_prefix" -a "x$result" = "xyes" ; then
        PYTHON_CFLAGS=-I$with_python_prefix/include/python$python_major_ver.$python_minor_ver
    else
        PYTHON_CFLAGS=""
    fi

    CFLAGS="$PYTHON_CFLAGS $CFLAGS"

    AC_TRY_COMPILE([
         #include <Python.h>
       ],
       [ ],
       [ ac_cv_python_valid=yes ], [ ac_cv_python_valid=no ])
    CFLAGS=$ac_save_CFLAGS

    if test "x$ac_cv_python_valid" = "xno" ; then
        AC_MSG_RESULT([python status: **** suitable version NOT FOUND])
    else
        AC_MSG_RESULT([python status: **** suitable version FOUND])
    fi
                                                                                
    AC_MSG_RESULT([python *required* version: $1])
    AC_MSG_RESULT([python *found* version: $PYTHON_VERSION])

    if test "x$ac_cv_python_valid" = "xyes" ; then
	PYTHON_INSTALL_PATH=$with_python_prefix
        ifelse([$2], , :, [$2])
    else
        PYTHON_CFLAGS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(PYTHON_INSTALL_PATH)
    AC_SUBST(PYTHON_VERSION)
    AC_SUBST(PYTHON_MAJOR)
    AC_SUBST(PYTHON_MINOR)
    AC_SUBST(PYTHON_BIN_PATH)
    AC_SUBST(PYTHON_CFLAGS)
])

