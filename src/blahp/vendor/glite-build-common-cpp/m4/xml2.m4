dnl Usage:
dnl AC_XML2(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for xml2, and defines
dnl - XML2_CFLAGS (compiler flags)
dnl - XML2_LIBS (linker flags, stripping and path)
dnl - XML2_STATIC_LIBS (linker flags, stripping and path for static library)
dnl - XML2_INSTALL_PATH
dnl prerequisites:

AC_DEFUN([AC_XML2],
[
  AC_ARG_WITH(xml2_prefix,
     [  --with-xml2-prefix=PFX       prefix where 'xml2' is installed.],
     [],
     with_xml2_prefix=${XML2_INSTALL_PATH:-/usr})

  AC_MSG_CHECKING([for XML2 installation at ${with_xml2_prefix:-/usr}])

  ac_save_CFLAGS=$CFLAGS
  ac_save_LIBS=$LIBS
  if test -n "$with_xml2_prefix"; then
     XML2_CFLAGS="-I$with_xml2_prefix/include/libxml2"
  else
     XML2_CFLAGS=""
  fi

  if test -n "$with_xml2_prefix" -a "$with_xml2_prefix" != "/usr" ; then
     XML2_LIBS="-L$with_xml2_prefix/lib"
  else
     XML2_LIBS=""
  fi

  XML2_LIBS="$XML2_LIBS -lxml2"
  CFLAGS="$XML2_CFLAGS $CFLAGS"
  LIBS="$XML2_LIBS $LIBS"

  AC_TRY_COMPILE([ #include <libxml/parser.h> ],
                 [ xmlInitParser() ],
                 [ ac_cv_xml2_valid=yes ], [ac_cv_xml2_valid=no ])
  CFLAGS=$ac_save_CFLAGS
  LIBS=$ac_save_LIBS
  AC_MSG_RESULT([$ac_cv_xml2_valid])

  if test x$ac_cv_xml2_valid = xyes ; then
     XML2_INSTALL_PATH="$with_xml2_prefix"
     XML2_STATIC_LIBS="$with_xml2_prefix/lib/libxml2.a"
     ifelse([$2], , :, [$2])
  else
     XML2_CFLAGS=""
     XML2_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_SUBST(XML2_CFLAGS)
  AC_SUBST(XML2_LIBS)
  AC_SUBST(XML2_INSTALL_PATH)
  AC_SUBST(XML2_STATIC_LIBS)
])
