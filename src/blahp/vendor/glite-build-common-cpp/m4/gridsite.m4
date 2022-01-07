dnl Usage:
dnl AC_GRIDSITE(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for gridsite package, and defines
dnl - GRIDSITE_INSTALL_PATH
dnl - GRIDSITE_CFLAGS        (compiler flags for use with standard openssl)
dnl - GRIDSITE_LIBS          (linker flags, stripping and path for use with standard openssl)
dnl - GRIDSITE_GLOBUS_CFLAGS (compiler flags for use with globus openssl)
dnl - GRIDSITE_GLOBUS_LIBS   (linker flags, stripping and path for use with globus openssl)
dnl
dnl The openssl compilation flags, which maybe used for compilation and linking
dnl against gridsite:
dnl - GRIDSITE_OPENSSL_CFLAGS        (compiler flags for use with standard openssl)
dnl - GRIDSITE_OPENSSL_LIBS          (linker flags, stripping and path for use with standard openssl)
dnl - GRIDSITE_OPENSSL_GLOBUS_CFLAGS (compiler flags for use with globus openssl)
dnl - GRIDSITE_OPENSSL_GLOBUS_LIBS   (linker flags, stripping and path for use with globus openssl)
dnl
dnl prerequisites:

AC_DEFUN([AC_GRIDSITE],
[
    AC_REQUIRE([AC_GLOBUS])
    AC_ARG_WITH(gridsite_prefix,
	[  --with-gridsite-prefix=PFX       prefix where 'gridsite' is installed.],
	[], 
        with_gridsite_prefix=${GRIDSITE_INSTALL_PATH:-/usr})

    ac_save_CFLAGS=$CFLAGS
    ac_save_LIBS=$LIBS

    dnl Set openssl flags: standard openssl and the distro from globus
    dnl GLOBUS_OPENSSL_CFLAGS="$GLOBUS_NOTHR_CFLAGS"
    dnl GLOBUS_OPENSSL_LIBS="-lssl_$GLOBUS_NOTHR_FLAVOR -lcrypto_$GLOBUS_NOTHR_FLAVOR"
    OPENSSL_CFLAGS="-I/usr/include/openssl -I/usr/kerberos/include"
    OPENSSL_LIBS="-lssl -lcrypto"

    dnl Determine the xml compilation flags
    which xml2-config 2>/dev/null
    if test $? -eq 0 ; then
        XML_CFLAGS=`xml2-config --cflags`
        XML_LIBS=`xml2-config --libs`
    else
        which pkg-config 2>/dev/null
        if test $? -eq 0 ; then
            XML_CFLAGS=`pkg-config --cflags libxml-2.0`
            XML_LIBS=`pkg-config --libs libxml-2.0`
        else
            XML_CFLAGS="-I/usr/include/libxml2"
            XML_LIBS="-lxml2 -lz -lm"
        fi
    fi
        

    dnl Set the gridsite compilation flags:
    dnl - with and without globus.
    dnl - openssl flags in separate variables

    if test -d "${with_gridsite_prefix}/lib64"; then    
	ac_gridsite_lib_dir="lib64"
    else
        ac_gridsite_lib_dir="lib"
    fi

    GRIDSITE_GLOBUS_CFLAGS="$XML_CFLAGS -I$with_gridsite_prefix/include"
    GRIDSITE_GLOBUS_LIBS="$XML_LIBS -L$with_gridsite_prefix/$ac_gridsite_lib_dir -lgridsite_globus"
    GRIDSITE_GLOBUS_OPENSSL_CFLAGS="$GLOBUS_OPENSSL_CFLAGS"
    GRIDSITE_GLOBUS_OPENSSL_LIBS="$GLOBUS_OPENSSL_LIBS"

    GRIDSITE_CFLAGS="$XML_CFLAGS -I$with_gridsite_prefix/include"
    GRIDSITE_LIBS="$XML_LIBS -L$with_gridsite_prefix/$ac_gridsite_lib_dir -lgridsite"
    GRIDSITE_OPENSSL_CFLAGS="$OPENSSL_CFLAGS"
    GRIDSITE_OPENSSL_LIBS="$OPENSSL_LIBS"

    dnl Check compilation using standard openssl gridsite flavour
    AC_MSG_CHECKING([for GRIDSITE installation at ${with_gridsite_prefix} using STANDARD openssl])

    CFLAGS="$GRIDSITE_CFLAGS $GRIDSITE_OPENSSL_CFLAGS $CFLAGS"
    LIBS="$GRIDSITE_LIBS $GRIDSITE_OPENSSL_LIBS $LIBS"
    AC_TRY_COMPILE([
            #include <libxml/tree.h>
            #include "gridsite.h"
            #include "gridsite-gacl.h"
        ],
        [
            GACLuser *  gacluser = NULL;
        ],
        [ ac_cv_gridsite_valid=yes ], [ ac_cv_gridsite_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS
    AC_MSG_RESULT([$ac_cv_gridsite_valid])

    if test x$ac_cv_gridsite_valid = xyes ; then
      GRIDSITE_INSTALL_PATH=$with_gridsite_prefix
      ifelse([$2], , :, [$2])
    else
      GRIDSITE_CFLAGS=""
      GRIDSITE_LIBS=""
      GRIDSITE_OPENSSL_CFLAGS=""
      GRIDSITE_OPENSSL_LIBS=""
      ifelse([$3], , :, [$3])
    fi

    dnl Check compilation using globus openssl gridsite flavour
    AC_MSG_CHECKING([for GRIDSITE installation at ${with_gridsite_prefix} using GLOBUS openssl])

    CFLAGS="$GRIDSITE_GLOBUS_CFLAGS $GRIDSITE_GLOBUS_OPENSSL_CFLAGS $CFLAGS"
    LIBS="$GRIDSITE_GLOBUS_LIBS $GRIDSITE_GLOBUS_OPENSSL_LIBS $LIBS"
    AC_TRY_COMPILE([
            #include <libxml/tree.h>
            #include "gridsite.h"
            #include "gridsite-gacl.h"
        ],
        [
            GACLuser *  gacluser = NULL;
        ],
        [ ac_cv_gridsite_valid=yes ], [ ac_cv_gridsite_valid=no ])
    CFLAGS=$ac_save_CFLAGS
    LIBS=$ac_save_LIBS
    AC_MSG_RESULT([$ac_cv_gridsite_valid])

    if test x$ac_cv_gridsite_valid = xyes ; then
      GRIDSITE_INSTALL_PATH=$with_gridsite_prefix
      ifelse([$2], , :, [$2])
    else
      GRIDSITE_GLOBUS_CFLAGS=""
      GRIDSITE_GLOBUS_LIBS=""
      GRIDSITE_GLOBUS_OPENSSL_CFLAGS=""
      GRIDSITE_GLOBUS_OPENSSL_LIBS=""
      ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GRIDSITE_INSTALL_PATH)
    AC_SUBST(GRIDSITE_GLOBUS_CFLAGS)
    AC_SUBST(GRIDSITE_GLOBUS_LIBS)
    AC_SUBST(GRIDSITE_GLOBUS_OPENSSL_CFLAGS)
    AC_SUBST(GRIDSITE_GLOBUS_OPENSSL_LIBS)
    AC_SUBST(GRIDSITE_CFLAGS)
    AC_SUBST(GRIDSITE_LIBS)
    AC_SUBST(GRIDSITE_OPENSSL_CFLAGS)
    AC_SUBST(GRIDSITE_OPENSSL_LIBS)
])
