dnl Usage:
dnl AC_GLITE_WMS_TP_BYPASS
dnl - GLITE_WMS_TP_BYPASS_LIBS

AC_DEFUN([AC_GLITE_WMS_TP_BYPASS],
[
    ac_glite_wms_tp_bypass_prefix=$GLITE_LOCATION

    if test -n "ac_glite_wms_tp_bypass_prefix" ; then
	dnl
	dnl 
	dnl
        ac_glite_wms_tp_bypass_lib="-L$ac_glite_wms_tp_bypass_prefix/lib"
        GLITE_WMS_TP_BYPASS_LIBS="$ac_glite_wms_tp_bypass_lib -lglite-wms-grid-console-agent"

	ifelse([$2], , :, [$2])
    else
	GLITE_WMS_TP_BYPASS_LIBS=""
	ifelse([$3], , :, [$3])
    fi

    AC_SUBST(GLITE_WMS_TP_BYPASS_LIBS)
])

