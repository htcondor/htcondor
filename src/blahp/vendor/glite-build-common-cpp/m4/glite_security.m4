dnl Usage:
dnl AC_GLITE_SECURITY
dnl - GLITE_SECURITY_RENEWAL_THR_LIBS
dnl - GLITE_SECURITY_RENEWAL_NOTHR_LIBS

dnl - GLITE_SECURITY_VOMS_LIBS 
dnl - GLITE_SECURITY_VOMS_THR_LIBS
dnl - GLITE_SECURITY_VOMS_NOTHR_LIBS
dnl - GLITE_SECURITY_VOMS_CPP_LIBS
dnl - GLITE_SECURITY_VOMS_CPP_THR_LIBS
dnl - GLITE_SECURITY_VOMS_CPP_NOTHR_LIBS
dnl - GLITE_SECURITY_VOMS_STATIC_LIBS
dnl - GLITE_SECURITY_VOMS_STATIC_THR_LIBS
dnl - GLITE_SECURITY_VOMS_STATIC_NOTHR_LIBS

dnl - SEC_LCAS_LIBS (linker flags, stripping and path for static library)

dnl - SEC_LCMAPS_LIBS (linker flags, stripping and path for static library)
dnl - SEC_LCMAPS_WITHOUT_GSI_LIBS
dnl - SEC_LCMAPS_RETURN_WITHOUT_GSI_LIBS

dnl - SEC_GSOAP_PLUGIN_262_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_262_NOTHR_LIBS

dnl - SEC_GSOAP_PLUGIN_276b_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_276b_NOTHR_LIBS

dnl - SEC_GSOAP_PLUGIN_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_NOTHR_LIBS

dnl - SEC_GSOAP_PLUGIN_STATIC_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_STATIC_262_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_STATIC_270_THR_LIBS

dnl - SEC_GSOAP_PLUGIN_STATIC_NOTHR_LIBS
dnl - SEC_GSOAP_PLUGIN_STATIC_262_NOTHR_LIBS
dnl - SEC_GSOAP_PLUGIN_STATIC_270_NOTHR_LIBS

dnl - SEC_GSOAP_PLUGIN_GSS_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_GSS_NOTHR_LIBS

dnl - SEC_GSOAP_PLUGIN_GSS_STATIC_THR_LIBS
dnl - SEC_GSOAP_PLUGIN_GSS_STATIC_NOTHR_LIBS

AC_DEFUN([AC_GLITE_SECURITY],
[
    ac_glite_security_prefix=$GLITE_LOCATION

    have_voms=no
    AC_VOMS([],have_voms=yes,have_voms=no)

    have_proxyrenewal=no
    AC_PROXYRENEWAL([],have_proxyrenewal=yes,have_proxyrenewal=no)

    if test "x$have_voms" = "xyes" -a "x$have_proxyrenewal" = "xyes"; then
        GLITE_SECURITY_VOMS_STATIC_LIBS=$VOMS_STATIC_LIBS
        GLITE_SECURITY_VOMS_LIBS=$VOMS_LIBS
        GLITE_SECURITY_VOMS_CPP_LIBS=$VOMS_CPP_LIBS
        GLITE_SECURITY_VOMS_THR_LIBS=$VOMS_THR_LIBS
        GLITE_SECURITY_VOMS_CPP_THR_LIBS=$VOMS_CPP_THR_LIBS
        GLITE_SECURITY_VOMS_NOTHR_LIBS=$VOMS_NOTHR_LIBS
        GLITE_SECURITY_VOMS_CPP_NOTHR_LIBS=$VOMS_CPP_NOTHR_LIBS
        GLITE_SECURITY_VOMS_STATIC_THR_LIBS=$VOMS_STATIC_THR_LIBS
        GLITE_SECURITY_VOMS_STATIC_NOTHR_LIBS=$VOMS_STATIC_NOTHR_LIBS

        GLITE_SECURITY_RENEWAL_THR_LIBS=$RENEWAL_THR_LIBS
        GLITE_SECURITY_RENEWAL_NOTHR_LIBS=$RENEWAL_NOTHR_LIBS
        ifelse([$2], , :, [$2])
    else
        GLITE_SECURITY_VOMS_LIBS=""
        GLITE_SECURITY_VOMS_CPP_LIBS=""
        GLITE_SECURITY_VOMS_STATIC_LIBS=""

        GLITE_SECURITY_RENEWAL_THR_LIBS=""
        GLITE_SECURITY_RENEWAL_NOTHR_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_SECURITY_VOMS_STATIC_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_CPP_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_STATIC_THR_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_STATIC_NOTHR_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_CPP_THR_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_CPP_NOTHR_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_THR_LIBS)
    AC_SUBST(GLITE_SECURITY_VOMS_NOTHR_LIBS)
    
    AC_SUBST(GLITE_SECURITY_RENEWAL_THR_LIBS)
    AC_SUBST(GLITE_SECURITY_RENEWAL_NOTHR_LIBS)
])

AC_DEFUN([AC_PROXYRENEWAL],
[ 
  AC_ARG_WITH(proxyrenewal-prefix,
        [  --with-proxyrenewal-prefix=PFX     prefix where proxyrenewal is installed. (/usr)],
        [],
        with_proxyrenewal_prefix=${GLITE_LOCATION:-/usr})

  AC_MSG_CHECKING([for PROXYRENEWAL installation at ${with_proxyrenewal_prefix}])
    
  ac_save_CFLAGS=$CFLAGS
  ac_save_CPPFLAGS=$CPPFLAGS
  ac_save_LIBS=$LIBS
  if test -n "$with_proxyrenewal_prefix" ; then
     if test "x$host_cpu" = "xx86_64"; then
         RENEWAL_LIBS="-L$with_proxyrenewal_prefix/lib64"
     else
         RENEWAL_LIBS="-L$with_proxyrenewal_prefix/lib"
     fi

     RENEWAL_CFLAGS="-I$with_proxyrenewal_prefix/include"
  else
     RENEWAL_CFLAGS=""
     RENEWAL_LIBS=""
  fi

  if test "x$GLOBUS_THR_FLAVOR" = "x" -a "x$GLOBUS_NOTHR_FLAVOR" = "x" ; then
    RENEWAL_THR_LIBS="$RENEWAL_LIBS -lglite_security_proxyrenewal -lglite_security_proxyrenewal_core"
    RENEWAL_NOTHR_LIBS="$RENEWAL_LIBS -lglite_security_proxyrenewal -lglite_security_proxyrenewal_core"   
  else
    RENEWAL_THR_LIBS="$RENEWAL_LIBS -lglite_security_proxyrenewal_$GLOBUS_THR_FLAVOR -lglite_security_proxyrenewal_core_$GLOBUS_THR_FLAVOR"
    RENEWAL_NOTHR_LIBS="$RENEWAL_LIBS -lglite_security_proxyrenewal_$GLOBUS_NOTHR_FLAVOR -lglite_security_proxyrenewal_core _$GLOBUS_NOTHR_FLAVOR"
  fi

  AC_LANG_SAVE
  AC_LANG_C
  CFLAGS="$RENEWAL_CFLAGS $CFLAGS"
  LIBS="$RENEWAL_THR_LIBS $LIBS"

  AC_TRY_COMPILE([ #include <glite/security/proxyrenewal/renewal.h> ],
                 [ edg_wlpr_ErrorCode error ],
                 [ ac_cv_renewal_thr_valid=yes ], [ac_cv_renewal_thr_valid=no ])
  CFLAGS=$ac_save_CFLAGS
  LIBS=$ac_save_LIBS
  AC_MSG_RESULT([$ac_cv_renewal_thr_valid for thr api])

  CPPFLAGS="$RENEWAL_CFLAGS $CPPFLAGS"
  LIBS="$RENEWAL_NOTHR_LIBS $LIBS"

  AC_TRY_COMPILE([ #include <glite/security/proxyrenewal/renewal.h> ],
                 [ edg_wlpr_ErrorCode error ],
                 [ ac_cv_renewal_nothr_valid=yes ], [ac_cv_renewal_nothr_valid=no ])
  CPPFLAGS=$ac_save_CPPFLAGS
  LIBS=$ac_save_LIBS
  AC_LANG_RESTORE
  AC_MSG_RESULT([$ac_cv_renewal_nothr_valid for nothr api])

  if test "x$ac_cv_renewal_nothr_valid" = "xyes" -a "x$ac_cv_renewal_thr_valid" = "xyes" ; then
     ifelse([$2], , :, [$2])
  else
     RENEWAL_THR_LIBS=""
     RENEWAL_NOTHR_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_SUBST(RENEWAL_THR_LIBS)
  AC_SUBST(RENEWAL_NOTHR_LIBS)

]
)

AC_DEFUN([AC_VOMS],
[
    have_voms_pkgconfig=no
    AC_VOMS_PKGCONFIG([],have_voms_pkgconfig=yes,have_voms_pkgconfig=no)
    
    if test "x$have_voms_pkgconfig" = "xyes" ; then
        ifelse([$2], , :, [$2])
    else
        have_voms_old=no
        AC_VOMS_OLD([],have_voms_old=yes,have_voms_old=no)
        
        if test "x$have_voms_old" = "xyes" ; then
            ifelse([$2], , :, [$2])
        else
            ifelse([$3], , :, [$3])
        fi
    fi
    
])

AC_DEFUN([AC_VOMS_PKGCONFIG],
[
    have_voms_pkgconfig=no
    PKG_CHECK_MODULES(VOMS, voms-2.0, have_voms_pkgconfig=yes, have_voms_pkgconfig=no)
    
    if test "x$have_voms_pkgconfig" = "xyes" ; then
        VOMS_THR_LIBS=${VOMS_LIBS}
        VOMS_NOTHR_LIBS=${VOMS_LIBS}
        VOMS_CPP_LIBS=${VOMS_LIBS}
        VOMS_CPP_THR_LIBS=${VOMS_LIBS}
        VOMS_CPP_NOTHR_LIBS=${VOMS_LIBS}
        ifelse([$2], , :, [$2])
    else
        VOMS_CFLAGS=""
        VOMS_LIBS=""
        VOMS_THR_LIBS=""
        VOMS_NOTHR_LIBS=""
        VOMS_CPP_LIBS=""
        VOMS_CPP_THR_LIBS=""
        VOMS_CPP_NOTHR_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(VOMS_CFLAGS)
    AC_SUBST(VOMS_LIBS)
    AC_SUBST(VOMS_CPP_LIBS)
    AC_SUBST(VOMS_THR_LIBS)
    AC_SUBST(VOMS_NOTHR_LIBS)
    AC_SUBST(VOMS_CPP_THR_LIBS)
    AC_SUBST(VOMS_CPP_NOTHR_LIBS)
])

AC_DEFUN([AC_VOMS_OLD],
[

  with_voms_prefix=$GLITE_LOCATION

  if test "x$host_cpu" = "xx86_64"; then
     library_path="lib64"
  else
     library_path="lib"
  fi

  if test -h "/usr/lib64" ; then
     library_path="lib"
  fi

  if ! test -e "/usr/lib64" ; then
     library_path="lib"
  fi

  AC_MSG_CHECKING([for VOMS installation at ${with_voms_prefix}])

  ac_save_CFLAGS=$CFLAGS
  ac_save_CPPFLAGS=$CPPFLAGS
  ac_save_LIBS=$LIBS
  if test -n "$with_voms_prefix" ; then
     VOMS_CFLAGS="-I$with_voms_prefix/include -I$with_voms_prefix/include/glite/security"
     VOMS_PATH_LIBS="-L$with_voms_prefix/$library_path"
  else
     VOMS_CFLAGS=""
     VOMS_PATH_LIBS=""
     VOMS_LIBS=""
     VOMS_THR_LIBS=""
     VOMS_NOTHR_LIBS=""
     VOMS_CPP_LIBS=""
     VOMS_CPP_THR_LIBS=""
     VOMS_CPP_NOTHR_LIBS=""
  fi

  VOMS_THR_LIBS="$VOMS_PATH_LIBS -lvomsc_$GLOBUS_THR_FLAVOR"
  VOMS_NOTHR_LIBS="$VOMS_PATH_LIBS -lvomsc_$GLOBUS_NOTHR_FLAVOR"

  VOMS_CPP_THR_LIBS="$VOMS_PATH_LIBS -lvomsapi_$GLOBUS_THR_FLAVOR"
  VOMS_CPP_NOTHR_LIBS="$VOMS_PATH_LIBS -lvomsapi_$GLOBUS_NOTHR_FLAVOR"

  VOMS_LIBS="$VOMS_PATH_LIBS -lvomsc"
  VOMS_CPP_LIBS="$VOMS_PATH_LIBS -lvomsapi"

  AC_LANG_SAVE
  AC_LANG_C
  CFLAGS="$GLOBUS_THR_CFLAGS $VOMS_CFLAGS $CFLAGS"
  LIBS="$VOMS_THR_LIBS $LIBS"

  AC_TRY_COMPILE([ #include <glite/security/voms/voms_apic.h> ],
                 [ struct vomsdata *voms_info = VOMS_Init("/tmp", "/tmp") ],
                 [ ac_cv_vomsc_valid=yes ], [ac_cv_vomsc_valid=no ])
  CFLAGS=$ac_save_CFLAGS
  LIBS=$ac_save_LIBS
  AC_MSG_RESULT([$ac_cv_vomsc_valid for c api])

  AC_LANG_CPLUSPLUS
  CPPFLAGS="$GLOBUS_THR_CFLAGS $VOMS_CFLAGS $CPPFLAGS"
  LIBS="$VOMS_CPP_THR_LIBS $LIBS"

  AC_TRY_COMPILE([ #include <glite/security/voms/voms_api.h> ],
                 [ vomsdata vo_data("","") ],
                 [ ac_cv_vomscpp_valid=yes ], [ac_cv_vomscpp_valid=no ])
  CPPFLAGS=$ac_save_CPPFLAGS
  LIBS=$ac_save_LIBS
  AC_LANG_RESTORE
  AC_MSG_RESULT([$ac_cv_vomscpp_valid for cpp api])

  if test "x$ac_cv_vomsc_valid" = "xyes" -a "x$ac_cv_vomscpp_valid" = "xyes" ; then
     VOMS_STATIC_LIBS="$with_voms_prefix/$library_path/libvomsc.a"
     VOMS_STATIC_THR_LIBS="$with_voms_prefix/$library_path/libvomsc_$GLOBUS_THR_FLAVOR.a"
     VOMS_STATIC_NOTHR_LIBS="$with_voms_prefix/$library_path/libvomsc_$GLOBUS_NOTHR_FLAVOR.a"
     ifelse([$2], , :, [$2])
  else
     VOMS_LIBS=""
     VOMS_THR_LIBS=""
     VOMS_NOTHR_LIBS=""
     VOMS_CPP_LIBS=""
     VOMS_CPP_THR_LIBS=""
     VOMS_CPP_NOTHR_LIBS=""
     VOMS_STATIC_LIBS=""
     VOMS_STATIC_THR_LIBS=""
     VOMS_STATIC_NOTHR_LIBS=""
     ifelse([$3], , :, [$3])
  fi

  AC_SUBST(VOMS_CFLAGS)
  AC_SUBST(VOMS_LIBS)
  AC_SUBST(VOMS_CPP_LIBS)
  AC_SUBST(VOMS_STATIC_LIBS)
  AC_SUBST(VOMS_THR_LIBS)
  AC_SUBST(VOMS_NOTHR_LIBS)
  AC_SUBST(VOMS_CPP_THR_LIBS)
  AC_SUBST(VOMS_CPP_NOTHR_LIBS)
  AC_SUBST(VOMS_STATIC_THR_LIBS)
  AC_SUBST(VOMS_STATIC_NOTHR_LIBS)
])


AC_DEFUN([AC_SEC_LCAS],
[
    with_lcas_prefix=$GLITE_LOCATION

    if test "x$host_cpu" = "xx86_64"; then
        library_path="lib64"
    else
        library_path="lib"
    fi

    if test -h "/usr/lib64" ; then
        library_path="lib"
    fi

    if ! test -e "/usr/lib64" ; then
        library_path="lib"
    fi

    if test -n "with_lcas_prefix" ; then
        dnl
        dnl
        dnl
        with_lcas_prefix_lib="-L$with_lcas_prefix/$library_path"
        SEC_LCAS_LIBS="$with_lcas_prefix_lib -llcas"

        ifelse([$2], , :, [$2])
    else
        SEC_LCAS_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(SEC_LCAS_LIBS)
])

AC_DEFUN([AC_SEC_LCMAPS],
[
    with_lcmaps_prefix=$GLITE_LOCATION

    if test "x$host_cpu" = "xx86_64"; then
        library_path="lib64"
    else
        library_path="lib"
    fi

    if test -h "/usr/lib64" ; then
        library_path="lib"
    fi

    if ! test -e "/usr/lib64" ; then
        library_path="lib"
    fi

    if test -n "with_lcmaps_prefix" ; then
        dnl
        dnl
        dnl
        with_lcmaps_prefix_lib="-L$with_lcmaps_prefix/$library_path"
        SEC_LCMAPS_CFLAGS="-I$with_lcmaps_prefix/include/glite/security -I$with_lcmaps_prefix/usr/include"
        SEC_LCMAPS_LIBS="$with_lcmaps_prefix_lib -llcmaps"

        ifelse([$2], , :, [$2])
    else
        SEC_LCMAPS_CFLAGS=""
        SEC_LCMAPS_LIBS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(SEC_LCMAPS_CFLAGS)
    AC_SUBST(SEC_LCMAPS_LIBS)
])

AC_DEFUN([AC_SEC_LCMAPS_WITHOUT_GSI],
[
    with_lcmaps_prefix=$GLITE_LOCATION

    if test "x$host_cpu" = "xx86_64"; then
        library_path="lib64"
    else
        library_path="lib"
    fi

    if test -h "/usr/lib64" ; then
        library_path="lib"
    fi

    if ! test -e "/usr/lib64" ; then
        library_path="lib"
    fi

    if test -n "with_lcmaps_prefix" ; then
        dnl
        dnl
        dnl

        if test -e "$with_lcmaps_prefix/include/glite/security/lcmaps_without_gsi" ; then
            SEC_LCMAPS_WITHOUT_GSI_CFLAGS="-DLCMAPSWITHOUTGSI -I$with_lcmaps_prefix/include/glite/security"
        else
            SEC_LCMAPS_WITHOUT_GSI_CFLAGS="-I$with_lcmaps_prefix/include/glite/security -I$with_lcmaps_prefix/usr/include"
        fi
        with_lcmaps_prefix_lib="-L$with_lcmaps_prefix/$library_path"
        SEC_LCMAPS_WITHOUT_GSI_LIBS="$with_lcmaps_prefix_lib -llcmaps_without_gsi"
        SEC_LCMAPS_RETURN_WITHOUT_GSI_LIBS="$with_lcmaps_prefix_lib -llcmaps_return_poolindex_without_gsi"

        ifelse([$2], , :, [$2])
    else
        SEC_LCMAPS_WITHOUT_GSI_LIBS=""
        SEC_LCMAPS_RETURN_WITHOUT_GSI_LIBS=""
        SEC_LCMAPS_WITHOUT_GSI_CFLAGS=""
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(SEC_LCMAPS_WITHOUT_GSI_CFLAGS)
    AC_SUBST(SEC_LCMAPS_WITHOUT_GSI_LIBS)
    AC_SUBST(SEC_LCMAPS_RETURN_WITHOUT_GSI_LIBS)
])

AC_DEFUN([AC_SEC_GSOAP_PLUGIN],
[
    AC_ARG_WITH(sec-gsoap-plugin-prefix,
        [  --with-sec-gsoap-plugin-prefix=PFX     prefix where gsoap plugin is installed. (/usr)],
        [],
        with_sec_gsoap_plugin_prefix=${GLITE_LOCATION:-/usr})

    GSOAP_VERSION=`${GSOAP_LOCATION}/bin/soapcpp2 -v 2>&1 | grep -Po "\d+\.\d+\.\d+"`
    if test -n "$GSOAP_VERSION" ; then
        GSOAP_VERSION_NUM=`echo $GSOAP_VERSION | awk -F . '{print $'1'*10000+$'2'*100+$'3'}'`
    else
        GSOAP_VERSION_NUM=0
    fi
 
    if test "x$GSOAP_VERSION_NUM" = "x20602" ; then
        gsoap_version_flavor="_262"
    elif test "x$GSOAP_VERSION_NUM" = "x20706" ; then
        gsoap_version_flavor="_276b"
    elif test "x$GSOAP_VERSION_NUM" = "x20709" ; then
        gsoap_version_flavor="_279d"
    elif test "x$GSOAP_VERSION_NUM" = "x20710" ; then
        gsoap_version_flavor="_2710_c"
    elif test "x$GSOAP_VERSION_NUM" = "x20713" ; then
        gsoap_version_flavor="_2713_cxx"
    elif test "x$GSOAP_VERSION_NUM" = "x20716" ; then
        gsoap_version_flavor="_2716_cxx"
    else
        gsoap_version_flavor=""
    fi

    if test "x$host_cpu" = "xx86_64"; then
        library_path="lib64"
    else
        library_path="lib"
    fi

    if test -h "/usr/lib64" ; then
        library_path="lib"
    fi

    if ! test -e "/usr/lib64" ; then
        library_path="lib"
    fi

    if test -n "$with_sec_gsoap_plugin_prefix" ; then
        dnl
        dnl
        dnl
        gsoap_plugin_library_path="$with_sec_gsoap_plugin_prefix/$library_path"
        with_sec_gsoap_plugin_lib="-L$gsoap_plugin_library_path"
        
        if test "x$GLOBUS_THR_FLAVOR" = "x" -a "x$GLOBUS_NOTHR_FLAVOR" = "x" ; then
            globus_thr_suffix=""
            globus_nothr_suffix=""
        else
            globus_thr_suffix="_${GLOBUS_THR_FLAVOR}"
            globus_nothr_suffix="_$GLOBUS_NOTHR_FLAVOR"
        fi

        SEC_GSOAP_PLUGIN_CFLAGS="-I$with_sec_gsoap_plugin_prefix/include"
        
        SEC_GSOAP_PLUGIN_262_THR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin_262${globus_thr_suffix}"
        SEC_GSOAP_PLUGIN_262_NOTHR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin_262${globus_nothr_suffix}"

        SEC_GSOAP_PLUGIN_STATIC_262_THR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin_262${globus_thr_suffix}.a"
        SEC_GSOAP_PLUGIN_STATIC_270_THR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin_270${globus_thr_suffix}.a"	

        SEC_GSOAP_PLUGIN_STATIC_262_NOTHR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin_262${globus_nothr_suffix}.a"
        SEC_GSOAP_PLUGIN_STATIC_270_NOTHR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin_270${globus_nothr_suffix}.a"

        SEC_GSOAP_PLUGIN_GSS_THR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gss${globus_thr_suffix}"
        SEC_GSOAP_PLUGIN_GSS_NOTHR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gss${globus_nothr_suffix}"

        SEC_GSOAP_PLUGIN_GSS_STATIC_THR_LIBS="$gsoap_plugin_library_path/libglite_security_gss${globus_thr_suffix}.a"
        SEC_GSOAP_PLUGIN_GSS_STATIC_NOTHR_LIBS="$gsoap_plugin_library_path/libglite_security_gss${globus_nothr_suffix}.a"

        SEC_GSOAP_PLUGIN_276b_THR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin_276b${globus_thr_suffix}"
        SEC_GSOAP_PLUGIN_276b_NOTHR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin_276b${globus_nothr_suffix}"

        SEC_GSOAP_PLUGIN_THR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin${gsoap_version_flavor}${globus_thr_suffix}"
        SEC_GSOAP_PLUGIN_NOTHR_LIBS="$with_sec_gsoap_plugin_lib -lglite_security_gsoap_plugin${gsoap_version_flavor}${globus_nothr_suffix}"
        SEC_GSOAP_PLUGIN_STATIC_THR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin${gsoap_version_flavor}${globus_thr_suffix}.a"
        SEC_GSOAP_PLUGIN_STATIC_NOTHR_LIBS="$gsoap_plugin_library_path/libglite_security_gsoap_plugin${gsoap_version_flavor}${globus_nothr_suffix}.a"

        ifelse([$2], , :, [$2])
    else
        SEC_GSOAP_PLUGIN_262_THR_LIBS=""
	    SEC_GSOAP_PLUGIN_262_NOTHR_LIBS=""

        SEC_GSOAP_PLUGIN_STATIC_THR_LIBS=""
        SEC_GSOAP_PLUGIN_STATIC_262_THR_LIBS=""
        SEC_GSOAP_PLUGIN_STATIC_270_THR_LIBS=""

        SEC_GSOAP_PLUGIN_STATIC_NOTHR_LIBS=""
        SEC_GSOAP_PLUGIN_STATIC_262_NOTHR_LIBS=""
        SEC_GSOAP_PLUGIN_STATIC_270_NOTHR_LIBS=""

        SEC_GSOAP_PLUGIN_GSS_THR_LIBS=""
        SEC_GSOAP_PLUGIN_GSS_NOTHR_LIBS=""

        SEC_GSOAP_PLUGIN_GSS_STATIC_THR_LIBS=""
        SEC_GSOAP_PLUGIN_GSS_STATIC_NOTHR_LIBS=""

        SEC_GSOAP_PLUGIN_276b_THR_LIBS=""
        SEC_GSOAP_PLUGIN_276b_NOTHR_LIBS=""
        
        SEC_GSOAP_PLUGIN_THR_LIBS=""
        SEC_GSOAP_PLUGIN_NOTHR_LIBS=""
        
        ifelse([$3], , :, [$3])
    fi

    AC_SUBST(SEC_GSOAP_PLUGIN_CFLAGS)
    AC_SUBST(SEC_GSOAP_PLUGIN_262_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_262_NOTHR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_262_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_270_THR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_NOTHR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_262_NOTHR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_STATIC_270_NOTHR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_GSS_STATIC_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_GSS_STATIC_NOTHR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_276b_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_276b_NOTHR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_GSS_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_GSS_NOTHR_LIBS)

    AC_SUBST(SEC_GSOAP_PLUGIN_THR_LIBS)
    AC_SUBST(SEC_GSOAP_PLUGIN_NOTHR_LIBS)
])

