dnl Usage:
dnl AC_MYPROXY(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - MYPROXY_NOTHR_CFLAGS
dnl - MYPROXY_THR_CFLAGS
dnl - MYPROXY_NOTHR_LIBS
dnl - MYPROXY_THR_LIBS
dnl
dnl
dnl AC_PROXYRENEWALCORE(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - GLITE_SECURITY_RENEWALCORE_THR_LIBS
dnl - GLITE_SECURITY_RENEWALCORE_NOTHR_LIBS

AC_DEFUN([AC_MYPROXY],
[
    have_myproxy_pkgconfig=no
    AC_MYPROXY_PKGCONFIG([],have_myproxy_pkgconfig=yes,have_myproxy_pkgconfig=no)
    
    if test "x$have_myproxy_pkgconfig" = "xyes" ; then
        ifelse([$2], , :, [$2])
    else
        have_myproxy_old=no
        AC_MYPROXY_OLD([],have_myproxy_old=yes,have_myproxy_old=no)
        
        if test "x$have_myproxy_old" = "xyes" ; then
            ifelse([$2], , :, [$2])
        else
            ifelse([$3], , :, [$3])
        fi
    fi
    
])

AC_DEFUN([AC_MYPROXY_PKGCONFIG],
[
    have_myproxy_pkgconfig=no
    PKG_CHECK_MODULES(MYPROXY, myproxy, have_myproxy_pkgconfig=yes, have_myproxy_pkgconfig=no)
    
    if test "x$have_myproxy_pkgconfig" = "xyes" ; then
        MYPROXY_THR_CFLAGS=$MYPROXY_CFLAGS
        MYPROXY_NOTHR_CFLAGS=$MYPROXY_CFLAGS
        MYPROXY_THR_LIBS=$MYPROXY_LIBS
        MYPROXY_NOTHR_LIBS=$MYPROXY_LIBS
        ifelse([$2], , :, [$2])
    else
        MYPROXY_THR_CFLAGS=""
        MYPROXY_NOTHR_CFLAGS=""
        MYPROXY_THR_LIBS=""
        MYPROXY_NOTHR_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(MYPROXY_NOTHR_CFLAGS)
    AC_SUBST(MYPROXY_THR_CFLAGS)
    AC_SUBST(MYPROXY_NOTHR_LIBS)
    AC_SUBST(MYPROXY_THR_LIBS)
])

AC_DEFUN([AC_MYPROXY_OLD],
[

  AC_ARG_WITH(myproxy_prefix,
      [  --with-myproxy-prefix=PFX     prefix where MyProxy is installed. (/opt/globus)],
      [],
      with_myproxy_prefix=${MYPROXY_LOCATION:-/opt/globus})

  AC_ARG_WITH(myproxy_nothr_flavor,
      [  --with-myproxy-nothr-flavor=flavor [default=gcc32dbg]],
      [],
      with_myproxy_nothr_flavor=${MYPROXY_FLAVOR:-gcc32dbg})

  AC_MSG_RESULT(["MyProxy nothread flavor is $with_myproxy_nothr_flavor"])

  AC_ARG_WITH(myproxy_thr_flavor,
      [  --with-myproxy-thr-flavor=flavor [default=gcc32dbgpthr]],
      [],
      with_myproxy_thr_flavor=${MYPROXY_FLAVOR:-gcc32dbgpthr})

  AC_MSG_RESULT(["MyProxy thread flavor is $with_myproxy_thr_flavor"])

  includedir_nothr="$with_myproxy_prefix/include/$with_myproxy_nothr_flavor"
  includedir_thr="$with_myproxy_prefix/include/$with_myproxy_thr_flavor"

  ac_myproxy_ldlib="$with_myproxy_prefix/lib"

  AC_MSG_CHECKING([for myproxy nothr])
  if test -f "$includedir_nothr/myproxy.h" -a \
          -f "$ac_myproxy_ldlib/libmyproxy_$with_myproxy_nothr_flavor.so" ; then
    MYPROXY_NOTHR_CFLAGS="-I$includedir_nothr"
    MYPROXY_NOTHR_LIBS="-L$ac_myproxy_ldlib -lmyproxy_$with_myproxy_nothr_flavor"
    AC_MSG_RESULT([yes])
  else
    MYPROXY_NOTHR_CFLAGS=""
    MYPROXY_NOTHR_LIBS=""
    AC_MSG_RESULT([no])
  fi

  AC_MSG_CHECKING([for myproxy thr])
  if test -f "$includedir_thr/myproxy.h" -a \
          -f "$ac_myproxy_ldlib/libmyproxy_$with_myproxy_thr_flavor.so" ; then
    MYPROXY_THR_CFLAGS="-I$includedir_thr"
    MYPROXY_THR_LIBS="-L$ac_myproxy_ldlib -lmyproxy_$with_myproxy_thr_flavor"
    AC_MSG_RESULT([yes])
  else
    MYPROXY_THR_CFLAGS=""
    MYPROXY_THR_LIBS=""
    AC_MSG_RESULT([no])
  fi

  if test -n "$MYPROXY_THR_LIBS" -a -n "$MYPROXY_NOTHR_LIBS" ; then
    ifelse([$2], , :, [$2])
  else
    ifelse([$3], , :, [$3])
  fi

  AC_SUBST(MYPROXY_NOTHR_CFLAGS)
  AC_SUBST(MYPROXY_THR_CFLAGS)
  AC_SUBST(MYPROXY_NOTHR_LIBS)
  AC_SUBST(MYPROXY_THR_LIBS)

])


