dnl Usage:
dnl AC_GLITE_WMS_RLS
dnl - GLITE_WMS_RLS_LIBS
dnl
dnl
dnl AC_GLITE_WMS_CHKPT
dnl - GLITE_WMS_CHKPT_LIBS
dnl
dnl
dnl AC_GLITE_WMS_PARTITIONER
dnl - GLITE_WMS_PARTITIONER_LIBS
dnl


AC_DEFUN([AC_GLITE_WMS_RLS],
[
    ac_glite_wms_rls_prefix=$GLITE_LOCATION

    if test -n "$ac_glite_wms_rls_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_wms_rls_lib="$GLITE_LDFLAGS"
    fi

    result=no

    if test -n "$ac_glite_wms_rls_lib" ; then
        dnl
        dnl Set macros
        dnl
        GLITE_WMS_RLS_LIBS="$ac_glite_wms_rls_lib -lglite_wms_replicaservice"
        result=yes
    fi

    if test x$result == xyes ; then
        dnl
        dnl
        dnl
        ifelse([$2], , :, [$2])
    else
        dnl
        dnl Unset macros
        dnl    
        GLITE_WMS_RLS_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_RLS_LIBS)
])

AC_DEFUN([AC_GLITE_WMS_CHKPT],
[
    ac_glite_wms_chkpt_prefix=$GLITE_LOCATION

    if test -n "$ac_glite_wms_chkpt_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_wms_chkpt_lib="$GLITE_LDFLAGS"
    fi

    result=no

    if test -n "$ac_glite_wms_chkpt_lib" ; then
        dnl
        dnl Set macros
        dnl
        GLITE_WMS_CHKPT_LIBS="$ac_glite_wms_chkpt_lib -lglite_wms_checkpointing"
        result=yes
    fi

    if test x$result == xyes ; then
        dnl
        dnl
        dnl
        ifelse([$2], , :, [$2])
    else
        dnl
        dnl Unset macros
        dnl
        GLITE_WMS_CHKPT_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_CHKPT_LIBS)
])

AC_DEFUN([AC_GLITE_WMS_PARTITIONER],
[
    ac_glite_wms_partitioner_prefix=$GLITE_LOCATION

    if test -n "$ac_glite_wms_partitioner_prefix" ; then
        dnl
        dnl
        dnl
        ac_glite_wms_partitioner_lib="$GLITE_LDFLAGS"
    fi

    result=no

    if test -n "$ac_glite_wms_partitioner_lib" ; then
        dnl
        dnl Set macros
        dnl
        GLITE_WMS_PARTITIONER_LIBS="$ac_glite_wms_partitioner_lib -lglite_wms_partitioner"
        result=yes
    fi

    if test x$result == xyes ; then
        dnl
        dnl
        dnl
        ifelse([$2], , :, [$2])
    else
        dnl
        dnl Unset macros
        dnl
        GLITE_WMS_PARTITIONER_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_PARTITIONER_LIBS)
])

AC_DEFUN([GLITE_CHECK_XSLT],
[AC_ARG_WITH(
    [xslt_prefix],
    [AS_HELP_STRING(
        [--with-xslt-prefix=PFX],
        [prefix where xslt is installed  @<:@default=/usr@:>@]
    )],
    [],
    [with_xslt_prefix=/usr])

    AC_MSG_CHECKING([for xslt])

    if test -f ${with_xslt_prefix}/include/libxslt/xslt.h; then
        XSLT_CPPFLAGS="-I${with_xslt_prefix}/include"
    
        if test "x${host_cpu}" = xx86_64 -o "x${host_cpu}" = xia64 ; then
            ac_xslt_lib_dir="lib64"
        else
            ac_xslt_lib_dir="lib"
        fi

        XSLT_LDFLAGS="-L${with_xslt_prefix}/${ac_xslt_lib_dir}"
        XSLT_LIBS="-lxslt"

        AC_MSG_RESULT([yes])
    else
        AC_MSG_ERROR([no])
    fi

    AC_SUBST(XSLT_CPPFLAGS)
    AC_SUBST(XSLT_LDFLAGS)
    AC_SUBST(XSLT_LIBS)

])

