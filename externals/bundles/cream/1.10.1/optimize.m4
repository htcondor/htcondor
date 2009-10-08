AC_DEFUN(AC_OPTIMIZE,
[
    AC_MSG_CHECKING([for OPTIMIZATION parameters])

    AC_ARG_ENABLE(optimization,
	[  --enable-optimization Enable optimization],
        [ if test "$enableval" = "yes"
          then
            optimizopt=$enableval
            if test "$GCC" = "yes"
            then
              optimise='-O2 '
            else
              optimise='-O '
            fi
          else
            optimizopt=$enableval
            if test "$GCC" = "yes"
            then
              optimise='-O0 '
            else
              optimise=''
            fi
         fi],
         optimizopt=$enableval
    )
  
    if test "x$optimizopt" = "xyes" ; then
        AC_MSG_RESULT(yes)
    else
        AC_MSG_RESULT(no)
    fi
  
    if test "x$optimizopt" = "xyes" ; then
        CXXFLAGS="-g -Wall $optimise"
        CFLAGS="-g -Wall $optimise"
    else
        CXXFLAGS="-g -Wall $optimise"
        CFLAGS="-g -Wall $optimise"
    fi

    AC_SUBST(CXXFLAGS)
    AC_SUBST(CFLAGS)
])

