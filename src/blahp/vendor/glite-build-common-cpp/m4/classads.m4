AC_DEFUN([GLITE_CHECK_CLASSADS],
[AC_ARG_WITH(
    [classads_prefix],
    [AS_HELP_STRING(
        [--with-classads-prefix=PFX],
        [prefix where libclassad is installed  @<:@default=/usr@:>@]
    )],
    [],
    [with_classads_prefix=/usr]
)
AC_MSG_CHECKING([for libclassad])
ac_classads_prefix=$with_classads_prefix
if test -d "$ac_classads_prefix"; then
    CLASSAD_CPPFLAGS="-I$ac_classads_prefix/include/classad -I$with_classads_prefix/include -DWANT_NAMESPACES -DWANT_CLASSAD_NAMESPACE"
    if test $host_cpu = x86_64 -o "$host_cpu" = ia64 ; then
        ac_classads_lib_dir="lib64"
    else
        ac_classads_lib_dir="lib"
    fi

    if test -h "/usr/lib64" ; then
        ac_classads_lib_dir="lib"
    fi
      
    if ! test -e "/usr/lib64" ; then
        ac_classads_lib_dir="lib"
    fi

    CLASSAD_LDFLAGS="-L$ac_classads_lib_dir"

    if test -e "$ac_classads_prefix/$ac_classads_lib_dir/libclassad_ns.a"; then
      CLASSAD_LIBS="-lclassad_ns"
    else
      CLASSAD_LIBS="-lclassad"
    fi
else
    AC_MSG_ERROR([$with_classads_prefix: no such directory])
fi

AC_LANG_PUSH([C++])
ac_save_cppflags=$CPPFLAGS
ac_save_ldflags=$LDFLAGS
ac_save_libs=$LIBS
CPPFLAGS="$CLASSAD_CPPFLAGS $CPPFLAGS"
LDFLAGS="$CLASSAD_LDFLAGS $LDFLAGS"
LIBS="$CLASSAD_LIBS $LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [@%:@include <classad_distribution.h>],
    [std::string const s("[key=\"value\"]");
     classad::ClassAd ad;
     classad::ClassAdParser parser;
     parser.ParseClassAd("[key=\"value\"]", ad);]
)],
[AC_MSG_RESULT([$ac_classads_prefix])],
[AC_MSG_ERROR([no])]
)

CPPFLAGS=$ac_save_cppflags
LDFLAGS=$ac_save_ldflags
LIBS=$ac_save_libs
AC_LANG_POP([C++])

AC_SUBST(CLASSAD_CPPFLAGS)
AC_SUBST(CLASSAD_LDFLAGS)
AC_SUBST(CLASSAD_LIBS)
])


dnl Usage:
dnl AC_CLASSAD(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl - CLASSAD_CFLAGS (compiler flags)
dnl - CLASSAD_LIBS (linker flags, stripping and path)
dnl - CLASSAD_DL_LIBS
dnl - CLASSAD_INSTALL_PATH
dnl - CLASSAD_PATH

AC_DEFUN([AC_CLASSADS],
[
    AC_ARG_WITH(classads_prefix, 
	[  --with-classads-prefix=PFX    prefix where the Classad is installed.],
	[], 
	with_classads_prefix="/usr")

    if test $host_cpu = x86_64 -o "$host_cpu" = ia64 ; then
        ac_classads_lib_dir="lib64"
    else
        ac_classads_lib_dir="lib"
    fi

    if test -h "/usr/lib64" ; then
        ac_classads_lib_dir="lib"
    fi
      
    if ! test -e "/usr/lib64" ; then
        ac_classads_lib_dir="lib"
    fi

    AC_MSG_RESULT([using lib path: $ac_classads_lib_dir])
  
    AC_MSG_CHECKING([for CLASSAD installation])

    CLASSAD_CFLAGS=""
    if test -e "$with_classads_prefix/$ac_classads_lib_dir/libclassad_ns.so"; then
      CLASSAD_LIBS="-lclassad_ns -ldl"
    else
      CLASSAD_LIBS="-lclassad -ldl"
    fi
    CLASSAD_DL_LIBS="-lclassad_dl"
    AC_MSG_RESULT([prefix: $with_classads_prefix])

    ac_classads_prefix=$with_classads_prefix

    CLASSAD_CFLAGS="-I$with_classads_prefix/include/classad -I$with_classads_prefix/include -DWANT_NAMESPACES -DWANT_CLASSAD_NAMESPACE"
    CLASSAD_LIBS="-L$with_classads_prefix/$ac_classads_lib_dir $CLASSAD_LIBS"
    CLASSAD_DL_LIBS="-L$with_classads_prefix/$ac_classads_lib_dir $CLASSAD_DL_LIBS"

    AC_LANG_SAVE
    AC_LANG_CPLUSPLUS
    ac_save_cppflags=$CPPFLAGS
    ac_save_libs=$LIBS
    CPPFLAGS="$CLASSAD_CFLAGS $CPPFLAGS"
    LIBS="$CLASSAD_LIBS $LIBS"
    AC_MSG_CHECKING([if a small classads program compiles])
    AC_TRY_LINK([ #include <classad_distribution.h> ],
		[ classad::ClassAd ad; classad::ClassAdParser parser; ],
		[ ac_have_classads=yes ], [ ac_have_classads=no ])
    if test x$ac_have_classads = xno ; then
        CLASSAD_CFLAGS="$CLASSAD_CFLAGS -DWANT_NAMESPACES"
        CPPFLAGS="$CLASSAD_CFLAGS $ac_save_cppflags"
        AC_TRY_LINK([ #include <classad_distribution.h> ],
                    [ classad::ClassAd ad; classad::ClassAdParser parser; ],
                    [ ac_have_classads=yes ], [ ac_have_classads=no ])	
    fi
    AC_MSG_RESULT([$ac_have_classads])

    CPPFLAGS=$ac_save_cppflags
    LIBS=$ac_save_libs
    AC_LANG_RESTORE

    CLASSAD_PATH=$with_classads_prefix

    if test x$ac_have_classads = xyes ; then
        CLASSAD_INSTALL_PATH=$ac_classads_prefix
	ifelse([$2], , :, [$2])
    else
        AC_MSG_WARN([
	    ***  Cannot compile a small classads program: check whether the
	    ***  Condor ClassADs library is installed])
        CLASSAD_CFLAGS=""
        CLASSAD_LIBS=""
        CLASSAD_DL_LIBS=""
	CLASSAD_PATH=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(CLASSAD_INSTALL_PATH)
    AC_SUBST(CLASSAD_CFLAGS)
    AC_SUBST(CLASSAD_LIBS)
    AC_SUBST(CLASSAD_DL_LIBS)
    AC_SUBST(CLASSAD_PATH)
])

