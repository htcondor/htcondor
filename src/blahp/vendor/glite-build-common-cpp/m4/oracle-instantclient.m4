
dnl AC_ORACLE_INSTANTCLIENT(MINIMUM-VERSION)
dnl define ORACLE_CFLAGS , ORACLE_OCI_LIBS, ORACLE_STATIC_OCCI_LIBS and ORACLE_OCCI_LIBS
dnl
AC_DEFUN([AC_ORACLE_INSTANTCLIENT],
[
  # Set Oracle InstantClient Properties
  AC_ARG_WITH(oracle_prefix,
        [  --with-oracle-prefix=PFX     prefix where Oracle Client is installed. (/usr)],
        [],
        with_oracle_prefix=${ORACLE_PATH:-/usr})

  AC_ARG_WITH(oracle_version,
        [  --with-oracle-version=PFX     Oracle Client version. (10.1.0.3)],
        [],
        with_oracle_version=${ORACLE_VERSION:-10.1.0.3})

  ORACLE_PATH="$with_oracle_prefix"
  ORACLE_VERSION="$with_oracle_version"
  if test -d "$with_oracle_prefix/include/oracle/$with_oracle_version/client"
  then
    # AC_MSG_RESULT([Setting 32-bit Oracle FLAGS and Libraries])
    ORACLE_CFLAGS="-I$with_oracle_prefix/include/oracle/$with_oracle_version/client -I$with_oracle_prefix/include"
    ac_oracle_ldlib="-L$with_oracle_prefix/lib/oracle/$with_oracle_version/client/lib"
  elif test -d "$with_oracle_prefix/include/oracle/$with_oracle_version/client64"
  then
    # AC_MSG_RESULT([Setting 64-bit Oracle FLAGS and Libraries])
    ORACLE_CFLAGS="-I$with_oracle_prefix/include/oracle/$with_oracle_version/client64 -I$with_oracle_prefix/include"
    ac_oracle_ldlib="-L$with_oracle_prefix/lib/oracle/$with_oracle_version/client64/lib"
  fi
  ORACLE_OCI_LIBS="$ac_oracle_ldlib -lclntsh -lnnz10 -lociei"
  ORACLE_STATIC_OCCI_LIBS="$ac_oracle_ldlib -lclntsh -lnnz10 -lociei -locci10"
  ORACLE_OCCI_LIBS="$ac_oracle_ldlib -lclntsh -lnnz10 -lociei -locci"

  AC_MSG_CHECKING([oracle installation])
  AC_MSG_RESULT([])

  CPPFLAGS_SAVE=$CPPFLAGS
  CPPFLAGS=$ORACLE_CFLAGS
  AC_CHECK_HEADER([oci.h], [ac_oracle_includes_valid=yes], [ac_oracle_includes_valid=no], [])
  CPPFLAGS=$CPPFLAGS_SAVE

  CPPFLAGS_SAVE=$CPPFLAGS
  CPPFLAGS=$ORACLE_OCI_LIBS
  AC_CHECK_LIB([ociei], [OCIEnvCreate], [ac_oracle_libraries_valid=yes], [ac_oracle_libraries_valid=no], [])
  CPPFLAGS=$CPPFLAGS_SAVE

  if test x$ac_oracle_includes_valid = xyes -a x$ac_oracle_libraries_valid = xyes ; then
    AC_MSG_RESULT([using oracle includes $ORACLE_CFLAGS])
    AC_MSG_RESULT([using oracle libraries $ac_oracle_ldlib])
  else
    AC_MSG_RESULT([cannot use oracle includes $ORACLE_CFLAGS])
    AC_MSG_RESULT([cannot use oracle libraries $ac_oracle_ldlib])
    ORACLE_PATH=""
    ORACLE_VERSION=""
    ORACLE_CFLAGS=""
    ORACLE_OCI_LIBS=""
    ORACLE_STATIC_OCCI_LIBS=""
    ORACLE_OCCI_LIBS=""
    AC_MSG_FAILURE([provide a valid oracle installation using --with-oracle-prefix and --with-oracle-version])
  fi

  AC_SUBST(ORACLE_PATH)
  AC_SUBST(ORACLE_VERSION)
  AC_SUBST(ORACLE_CFLAGS)
  AC_SUBST(ORACLE_OCI_LIBS)
  AC_SUBST(ORACLE_STATIC_OCCI_LIBS)
  AC_SUBST(ORACLE_OCCI_LIBS)
])
