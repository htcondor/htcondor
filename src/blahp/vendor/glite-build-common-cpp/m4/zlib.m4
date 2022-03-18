dnl 
dnl Set ZLib Properties
dnl
AC_DEFUN([AC_ZLIB],
[
  # Set ZLib Properties
  AC_ARG_WITH(zlib_prefix,
    [  --with-zlib-prefix=PFX     prefix where ZLIB is installed. (/usr)],
    [],
    with_zlib_prefix=${ZLIBROOT:-/usr})
 
  AC_MSG_CHECKING([for ZLIB installation at ${with_zlib_prefix}])
 
  ac_save_CFLAGS=$CFLAGS
  ac_save_LIBS=$LIBS
      
  ZLIBROOT="$with_zlib_prefix"
 
  if test -n "$with_zlib_prefix"; then
     ZLIB_CFLAGS="-I$with_zlib_prefix/include"
  else
     ZLIB_CFLAGS=""
  fi

  if test -n "$with_zlib_prefix"; then
     if test "x$host_cpu" = "xx86_64"; then
         ZLIB_LIBS="-L$with_zlib_prefix/lib64"
     else
         ZLIB_LIBS="-L$with_zlib_prefix/lib"
     fi
  else
     ZLIB_LIBS=""
  fi

  ZLIB_LIBS="$ZLIB_LIBS -lz"
 
  CFLAGS="$ZLIB_CFLAGS $CFLAGS"
  LIBS="$ZLIB_LIBS $LIBS"
  AC_TRY_COMPILE([ #include <zlib.h> ],
                 [ z_stream s ],
                 [ ac_cv_zlib_valid=yes ], [ac_cv_zlib_valid=no ])
  CFLAGS=$ac_save_CFLAGS
  LIBS=$ac_save_LIBS
  AC_MSG_RESULT([$ac_cv_zlib_valid])

  if test x$ac_cv_zlib_valid = xyes ; then
     ifelse([$2], , :, [$2])
  else
     ZLIB_CFLAGS=""
     ZLIB_LIBS=""
     ifelse([$3], , :, [$3])
  fi
 
  AC_SUBST(ZLIB_CFLAGS)
  AC_SUBST(ZLIB_LIBS)
])
