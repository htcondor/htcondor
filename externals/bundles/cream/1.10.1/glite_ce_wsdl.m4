dnl Usage:
dnl AC_GLITE_CE_WSDL
dnl - GLITE_CREAM_WSDL
dnl - GLITE_CREAM_WSDL_DEPS
dnl - GLITE_CEMON_CLIENT_WSDL
dnl - GLITE_CEMON_CLIENT_WSDL_DEPS
dnl - GLITE_CEMON_CONSUMER_WSDL
dnl - GLITE_CEMON_CONSUMER_WSDL_DEPS


AC_DEFUN([AC_GLITE_CE_WSDL],
[

    AC_ARG_WITH(glite_location,
        [  --with-glite-location=PFX     prefix where GLITE is installed. (/opt/glite)],
        [],
        with_glite_location=/opt/glite)

    AC_ARG_WITH(ce-wsdl-version,
        [  --with-ce-wsdl-version=version   version of the CE wsdl],
        [],
        with_ce_wsdl_version)

    AC_ARG_WITH(delegation-wsdl-version,
        [  --with-delegation-wsdl-version=version   version of the CE wsdl],
        [],
        with_delegation_wsdl_version)

    AC_MSG_RESULT([WSDL VERSION: $with_ce_wsdl_version])
    AC_MSG_RESULT([DELEGATION VERSION: $with_delegation_wsdl_version])

    GLITE_CREAM_WSDL=$with_glite_location/interface/org.glite.ce-cream2_service-$with_ce_wsdl_version.wsdl
    GLITE_CREAM_WSDL_DEPS="$GLITE_CREAM_WSDL \
                           $with_glite_location/interface/www.gridsite.org-delegation-$with_delegation_wsdl_version.wsdl"

    GLITE_CEMON_CLIENT_WSDL=$with_glite_location/interface/org.glite.ce-monitor_service-$with_ce_wsdl_version.wsdl
    GLITE_CEMON_CLIENT_WSDL_DEPS="$GLITE_CEMON_CLIENT_WSDL \
                                  $with_glite_location/interface/org.glite.ce-monitor_types-$with_ce_wsdl_version.wsdl \
                                  $with_glite_location/interface/org.glite.ce-monitor_faults-$with_ce_wsdl_version.wsdl \
                                  $with_glite_location/interface/org.glite.ce-faults-$with_ce_wsdl_version.xsd"

    GLITE_CEMON_CONSUMER_WSDL=$with_glite_location/interface/org.glite.ce-monitor_consumer_service-$with_ce_wsdl_version.wsdl
    GLITE_CEMON_CONSUMER_WSDL_DEPS="$GLITE_CEMON_CONSUMER_WSDL \
                                    $with_glite_location/interface/org.glite.ce-monitor_types-$with_ce_wsdl_version.wsdl \
                                    $with_glite_location/interface/org.glite.ce-monitor_faults-$with_ce_wsdl_version.wsdl \
                                    $with_glite_location/interface/org.glite.ce-faults-$with_ce_wsdl_version.xsd"

    AC_SUBST(GLITE_CREAM_WSDL)
    AC_SUBST(GLITE_CREAM_WSDL_DEPS)
    AC_SUBST(GLITE_CEMON_CLIENT_WSDL)
    AC_SUBST(GLITE_CEMON_CLIENT_WSDL_DEPS)
    AC_SUBST(GLITE_CEMON_CONSUMER_WSDL)
    AC_SUBST(GLITE_CEMON_CONSUMER_WSDL_DEPS)
])
