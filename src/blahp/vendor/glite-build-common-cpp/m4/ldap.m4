dnl Usage:
dnl GLITE_CHECK_LDAP

dnl Exported variables (i.e. available to the various Makefile.am):
dnl LDAP_CPPFLAGS
dnl LDAP_LDFLAGS
dnl LDAP_LIBS

AC_DEFUN([GLITE_CHECK_LDAP],
[AC_ARG_WITH(
    [ldap_prefix],
    [AS_HELP_STRING(
        [--with-ldap-prefix=PFX],
        [prefix where ldap is installed  @<:@default=/usr@:>@]
    )],
    [],
    [with_ldap_prefix=/usr]
)

AC_MSG_CHECKING([for ldap])

if test -f ${with_ldap_prefix}/include/ldap.h; then
    LDAP_CPPFLAGS="-I${with_ldap_prefix}/include -DLDAP_DEPRECATED"
    
    if test -d "${with_ldap_prefix}/lib64"; then
        ac_ldap_lib_dir="lib64"
    else
        ac_ldap_lib_dir="lib"
    fi

    LDAP_LDFLAGS="-L${with_ldap_prefix}/${ac_ldap_lib_dir}"
    LDAP_LIBS="-lldap"
else
    AC_MSG_ERROR([no])
fi

AC_LANG_PUSH([C++])
ac_save_cppflags=$CPPFLAGS
ac_save_ldflags=$LDFLAGS
ac_save_libs=$LIBS
CPPFLAGS="$LDAP_CPPFLAGS $CPPFLAGS"
LDFLAGS="$LDAP_LDFLAGS $LDFLAGS"
LIBS="$LDAP_LIBS $LIBS"
AC_LINK_IFELSE([AC_LANG_PROGRAM(
    [@%:@include <ldap.h>],
    [LDAP* one = 0; void* two = 0;]
    [int ret = ldap_get_option(one, 0, two);])],
    [AC_MSG_RESULT([yes $with_ldap_prefix])],
    [AC_MSG_ERROR([no])]
)
CPPFLAGS=${ac_save_cppflags}
LDFLAGS=${ac_save_ldflags}
LIBS=${ac_save_libs}
AC_LANG_POP([C++])

AC_SUBST(LDAP_CPPFLAGS)
AC_SUBST(LDAP_LDFLAGS)
AC_SUBST(LDAP_LIBS)
])

