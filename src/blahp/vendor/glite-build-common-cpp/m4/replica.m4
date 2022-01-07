dnl Usage:
dnl AC_REPLICA(MINIMUM-VERSION, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]])
dnl Test for rm, and defines
dnl - REPLICA_MANAGER_CFLAGS (compiler flags)
dnl - REPLICA_MANAGER_LIBS (linker flags, stripping and path)
dnl - REPLICA_LOCATION_CFLAGS
dnl - REPLICA_LOCATION_LIBS
dnl - REPLICA_METADATA_CFLAGS
dnl - REPLICA_METADATA_LIBS
dnl - REPLICA_OPTIMIZATION_CFLAGS
dnl - REPLICA_OPTIMIZATION_LIBS
dnl - REPLICA_LIBS
dnl prerequisites:

AC_DEFUN([AC_REPLICA],
[
    EDG=opt/edg

    AC_ARG_WITH(replica_manager_prefix, 
	[  --with-replica-manager-prefix=PFX   prefix where 'replica manager' is installed.],
	[], 
	with_replica_manager_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for RESOURCE MANAGER installation at ${with_replica_manager_prefix}])

    if test -n "$with_replica_manager_prefix" -a "$with_replica_manager_prefix" != "/usr" ; then
	REPLICA_MANAGER_CFLAGS="-I$with_replica_manager_prefix/include"
        REPLICA_MANAGER_LIBS="-L$with_replica_manager_prefix/lib"
    else
	REPLICA_MANAGER_CFLAGS=""
        REPLICA_MANAGER_LIBS=""
    fi
  
    REPLICA_MANAGER_LIBS="$REPLICA_MANAGER_LIBS -ledg_replica_manager_client_gcc3_2_2"

    if test ! -f "$with_replica_manager_prefix/include/EdgReplicaManager/ReplicaManagerImpl.h"; then
        ac_replica_man=no
    else
        ac_replica_man=yes
    fi

    AC_ARG_WITH(replica_metadata_prefix,
        [  --with-replica-metadata-prefix=PFX   prefix where 'replica metadata' is installed.],
        [],
        with_replica_metadata_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for RESOURCE METADATA installation at ${with_replica_metadata_prefix}])

    if test -n "$with_replica_metadata_prefix" -a "$with_replica_metadata_prefix" != "/usr" ; then
        REPLICA_METADATA_CFLAGS="-I$with_replica_metadata_prefix/$EDG/include"
        REPLICA_METADATA_LIBS="-L$with_replica_metadata_prefix/$EDG/lib"
    else
        REPLICA_METADATA_CFLAGS=""
        REPLICA_METADATA_LIBS=""
    fi

    REPLICA_METADATA_LIBS="$REPLICA_METADATA_LIBS -ledg_replica_metadata_catalog_client_gcc3_2_2"

    AC_ARG_WITH(replica_location_prefix,
        [  --with-replica-location-prefix=PFX   prefix where 'replica location' is installed.],
        [],
        with_replica_location_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for RESOURCE LOCATION installation at ${with_replica_location_prefix}])

    if test -n "$with_replica_location_prefix" -a "$with_replica_location_prefix" != "/usr" ; then
        REPLICA_LOCATION_CFLAGS="-I$with_replica_location_prefix/$EDG/include"
        REPLICA_LOCATION_LIBS="-L$with_replica_location_prefix/$EDG/lib"
    else
        REPLICA_LOCATION_CFLAGS=""
        REPLICA_LOCATION_LIBS=""
    fi

    REPLICA_LOCATION_LIBS="$REPLICA_LOCATION_LIBS -ledg_local_replica_catalog_client_gcc3_2_2"

    AC_ARG_WITH(replica_optimization_prefix,
        [  --with-replica-optimization-prefix=PFX   prefix where 'replica optimization' is installed.],
        [],
        with_replica_optimization_prefix=${GLITE_LOCATION:-/opt/glite})

    AC_MSG_CHECKING([for RESOURCE OPTIMIZATION installation at ${with_replica_optimization_prefix}])

    if test -n "$with_replica_optimization_prefix" -a "$with_replica_optimization_prefix" != "/usr" ; then
        REPLICA_OPTIMIZATION_CFLAGS="-I$with_replica_optimization_prefix/$EDG/include"
        REPLICA_OPTIMIZATION_LIBS="-L$with_replica_optimization_prefix/$EDG/lib"
    else
        REPLICA_OPTIMIZATION_CFLAGS=""
        REPLICA_OPTIMIZATION_LIBS=""
    fi

    REPLICA_OPTIMIZATION_LIBS="$REPLICA_OPTIMIZATION_LIBS -ledg_replica_optimization_client_gcc3_2_2"

    REPLICA_LIBS="$REPLICA_MANAGER_LIBS $REPLICA_LOCATION_LIBS $REPLICA_METADATA_LIBS $REPLICA_OPTIMIZATION_LIBS"

    if test x$ac_replica_man = xyes; then
	ifelse([$2], , :, [$2])
    else
	REPLICA_MANAGER_CFLAGS=""
	REPLICA_MANAGER_LIBS=""
	REPLICA_LOCATION_CFLAGS=""
        REPLICA_LOCATION_LIBS=""
	REPLICA_METADATA_CFLAGS=""
        REPLICA_METADATA_LIBS=""
	REPLICA_OPTIMIZATION_CFLAGS=""
        REPLICA_OPTIMIZATION_LIBS=""
	REPLICA_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(REPLICA_MANAGER_CFLAGS)
    AC_SUBST(REPLICA_MANAGER_LIBS)
    AC_SUBST(REPLICA_LOCATION_CFLAGS)
    AC_SUBST(REPLICA_LOCATION_LIBS)
    AC_SUBST(REPLICA_METADATA_CFLAGS)
    AC_SUBST(REPLICA_METADATA_LIBS)
    AC_SUBST(REPLICA_OPTIMIZATION_CFLAGS)
    AC_SUBST(REPLICA_OPTIMIZATION_LIBS)
    AC_SUBST(REPLICA_LIBS)
])

