dnl 
dnl Set LibTar Properties
dnl

AC_DEFUN([AC_LIBTAR],
[
  # Set LibTar Properties
  AC_ARG_WITH(libtar_prefix,
    [  --with-libtar-prefix=PFX     prefix where LIBTAR is installed. (/usr)],
    [],
    with_libtar_prefix=${LIBTARROOT:-/usr})

  AC_MSG_CHECKING([for LIBTAR installation at ${with_libtar_prefix}])

  ac_save_CFLAGS=$CFLAGS
  ac_save_LIBS=$LIBS

  LIBTARROOT="$with_libtar_prefix"

  if test -n "$with_libtar_prefix"; then
     LIBTAR_CFLAGS="-I$with_libtar_prefix/include"
  else
     LIBTAR_CFLAGS=""
  fi

  if test -n "$with_libtar_prefix"; then
     if test "x$host_cpu" = "xx86_64"; then
         LIBTAR_LIBS="-L$with_libtar_prefix/lib64"
     else
         LIBTAR_LIBS="-L$with_libtar_prefix/lib"
     fi
  else
     LIBTAR_LIBS=""
  fi

  LIBTAR_LIBS="$LIBTAR_LIBS -ltar"

  CFLAGS="$LIBTAR_CFLAGS $CFLAGS"
  LIBS="$LIBTAR_LIBS $LIBS"
  AC_TRY_COMPILE([ #include <libtar.h> ],
                 [ tartype_t s ],
                 [ ac_cv_libtar_valid=yes ], [ac_cv_libtar_valid=no ])
  CFLAGS=$ac_save_CFLAGS
  LIBS=$ac_save_LIBS
  AC_MSG_RESULT([$ac_cv_libtar_valid])

  if test x$ac_cv_libtar_valid = xyes ; then
     ifelse([$2], , :, [$2])
  else
     LIBTAR_CFLAGS=""
     LIBTAR_LIBS=""
     ifelse([$3], , :, [$3])
  fi
    
  AC_SUBST(LIBTAR_CFLAGS)
  AC_SUBST(LIBTAR_LIBS)
])
