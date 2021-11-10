dnl Usage:
dnl AC_GSOAP_BASE(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - GSOAP_BASE_CFLAGS (compiler flags)
dnl - GSOAP_BASE_LIBS (linker flags, stripping and path)
dnl prerequisites:

AC_DEFUN([AC_GSOAP_BASE],
[
    EDG=/opt/edg

    AC_ARG_WITH(gsoap_base_prefix, 
	[  --with-gsoap-base-prefix=PFX   prefix where 'gsoap base' is installed.],
	[], 
	with_gsoap_base_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for GSOAP_BASE installation at ${with_gsoap_base_prefix}])

    if test -n "$with_gsoap_base_prefix" -a "$with_gsoap_base_prefix" != "/usr" ; then
	GSOAP_BASE_CFLAGS="-I$with_gsoap_base_prefix/$EDG/include"
        GSOAP_BASE_LIBS="-L$with_gsoap_base_prefix/$EDG/lib"
    else
	GSOAP_BASE_CFLAGS=""
        GSOAP_BASE_LIBS=""
    fi
  
    GSOAP_BASE_LIBS="$GSOAP_BASE_LIBS -ledg_gsoap_base_gcc3_2_2"

dnl    if test ! -f "$with_replica_manager_prefix/$EDG/include/EdgReplicaManager/ReplicaManagerImpl.h" ; then
dnl        ac_replica_man=no
dnl        AC_MSG_RESULT([no])
dnl    else
dnl        ac_replica_man=yes
dnl        AC_MSG_RESULT([yes])
dnl    fi

    ac_gsoap_base=yes

    if test x$ac_gsoap_base = xyes; then
	ifelse([$2], , :, [$2])
    else
	GSOAP_BASE_CFLAGS=""
	GSOAP_BASE_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GSOAP_BASE_CFLAGS)
    AC_SUBST(GSOAP_BASE_LIBS)
])

