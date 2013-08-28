 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # 
 # Licensed under the Apache License, Version 2.0 (the "License"); you 
 # may not use this file except in compliance with the License.  You may 
 # obtain a copy of the License at 
 # 
 #    http://www.apache.org/licenses/LICENSE-2.0 
 # 
 # Unless required by applicable law or agreed to in writing, software 
 # distributed under the License is distributed on an "AS IS" BASIS, 
 # WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 # See the License for the specific language governing permissions and 
 # limitations under the License. 
 # 
 ############################################################### 

if (LINUX)
  option( WITH_CREAM "Compiling with support for CREAM" ON )
endif(LINUX)
  
if ( WITH_CREAM )

if ( NOT PROPER )

	if (NOT AUTOCONF OR NOT AUTOMAKE OR NOT LIBTOOLIZE)
		message(FATAL_ERROR "Can't find autoconf, automake, or libtoolize, needed to build cream external!")
	endif()

	if (PREFER_CPP11)
		set( CPP11_FLAG -std=c++11 )
	else()
		set( CPP11_FLAG "" )
	endif()

		# we define cream 1st because other externals will place bits there.
		condor_pre_external( CREAM org.glite.ce.cream-client-api-c-1.12.1-14-p8 "lib;include;include/glite/security;opt" "done")

#########################################
		condor_pre_external( CREAMSOAP gsoap_2.7.6b-p8 "lib;include" "done")
		ExternalProject_Add(creamsoap
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CREAMSOAP_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/gsoap_2.7.6b.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gsoap_cast.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${CREAM_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND make
				    install && cp -f soapcpp2/wsdl/stlvector.h
				    ${CREAM_INSTALL_LOC}/include && touch
				    ${CREAMSOAP_INSTALL_LOC}/done )

		condor_post_external( creamsoap OFF OFF )

#########################################

		condor_pre_external( CARES c-ares-1.3.0-p8 "lib;include" "done")
		ExternalProject_Add(cares
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CARES_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/c-ares-1.3.0.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${CREAM_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND make
				    install && touch ${CARES_INSTALL_LOC}/done )
		condor_post_external( cares OFF OFF )

#########################################

		condor_pre_external( LOG4CPP log4cpp-1.0-3 "lib;include" "lib/liblog4cpp.a")
		ExternalProject_Add(log4cpp
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${LOG4CPP_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/${LOG4CPP_VER}.tar.gz
					#--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/log4errors.patch &&
					  patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/log4configure.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${LOG4CPP_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make && make check
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${LOG4CPP_INSTALL_LOC}
				    INSTALL_COMMAND make install )
		condor_post_external( log4cpp include OFF )

######################################### cream/gss packages.

		condor_pre_external( GRIDSITE gridsite-1.6.0-p8 "lib;include" "done")
		if ( BUILDING_LIBXML2 )
			set (GRIDSITE_DEPENDS ${LIBXML2_REF})
			set (GRIDSITE_INCLUDE "-I. -I../interface -I${LIBXML2_INSTALL_LOC}/include/libxml2")
		else ()
			set (GRIDSITE_DEPENDS "")
			set (GRIDSITE_INCLUDE "-I. -I../interface")
		endif ()
		ExternalProject_Add(gridsite
				    DEPENDS ${GRIDSITE_DEPENDS}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${GRIDSITE_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/gridsite-1.6.0.src.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gridsite_extern.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND export MYCFLAGS=${GRIDSITE_INCLUDE} &&
					  cd src &&
					  make libgridsite.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND cd src &&
				    cp -f libgridsite.a ${CREAM_INSTALL_LOC}/lib &&
				    cp -f ../interface/gridsite-gacl.h ../interface/gridsite.h ${CREAM_INSTALL_LOC}/include &&
				    touch ${GRIDSITE_INSTALL_LOC}/done )
		condor_post_external( gridsite OFF OFF )

#########################################

		condor_pre_external( GLITE_SECURITY org.glite.security.gss-2.0.1-1-p8 "lib;include" "done")
		ExternalProject_Add(glite_security
				    DEPENDS ${GLOBUS_REF} ${GRIDSITE_REF} ${CARES_REF}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${GLITE_SECURITY_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/org.glite.security.gss-2.0.1-1.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gss_import.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND cd src &&
					  gcc -g -O0 -W -Wall
					  -Wno-unused-parameter -I../interface
					  -I${CREAM_INSTALL_LOC}/include -I${GLOBUS_INSTALL_LOC}/include/globus -o glite_gss.thr.o -c glite_gss.c && 
					  ar crv libglite_security_gss_${GLOBUS_FLAVOR}pthr.a glite_gss.thr.o &&
					  ranlib libglite_security_gss_${GLOBUS_FLAVOR}pthr.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND cd src &&
					  cp ../interface/glite_gss.h ${CREAM_INSTALL_LOC}/include/glite/security &&
					  cp libglite_security_gss_${GLOBUS_FLAVOR}pthr.a ${CREAM_INSTALL_LOC}/lib && 
					  touch ${GLITE_SECURITY_INSTALL_LOC}/done )
		condor_post_external( glite_security OFF OFF )

#########################################

		# build gsoap plugin.
		set (CSP_CC_FLAGS -g -O0 -W -Wall -Wno-unused-parameter -I. -I../interface -DWITH_NONAMESPACES -I${CREAM_INSTALL_LOC}/include -D_GNU_SOURCE)
		condor_pre_external( CREAM_SOAP_PLUGIN org.glite.security.gsoap-plugin-2.0.1-3-p8 "lib;include" "done")
		ExternalProject_Add(cream_soap_plugin
					DEPENDS ${CREAMSOAP_REF} ${GLITE_SECURITY_REF}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CREAM_SOAP_PLUGIN_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/org.glite.security.gsoap-plugin-2.0.1-3.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd src &&
				    	${CMAKE_CURRENT_SOURCE_DIR}/cream_soap_plugin_patch.sh
				    #--Build Step ----------
				    BUILD_COMMAND cd src &&
						gcc ${CSP_CC_FLAGS} -o glite_gsplugin.thr.o -c glite_gsplugin.c &&
						gcc ${CSP_CC_FLAGS} -o stdsoap2.thr.o -c stdsoap2.c &&
						ar crv libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a glite_gsplugin.thr.o stdsoap2.thr.o &&
						ranlib libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}/lib
				    INSTALL_COMMAND  cd src &&
						cp libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a ${CREAM_INSTALL_LOC}/lib && 
						cp ../interface/glite_gscompat.h ../interface/glite_gsplugin.h ../interface/glite_gsplugin-int.h ${CREAM_INSTALL_LOC}/include/glite/security && 
						touch ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/done )
		condor_post_external( cream_soap_plugin OFF OFF )

		condor_pre_external( CREAM_WSDL org.glite.ce.wsdl-1.12.1-14-p8 "lib;include" "done")
		ExternalProject_Add(cream_wsdl
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CREAM_WSDL_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/org.glite.ce.wsdl-1.12.1-14.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "nothing to do"
				    #--Build Step ----------
				    BUILD_COMMAND project/install.sh ${CREAM_INSTALL_LOC} ${CREAM_INSTALL_LOC} 1.12.1 2.0.0
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND ln -fs share/wsdl ${CREAM_INSTALL_LOC}/interface && touch ${CREAM_WSDL_INSTALL_LOC}/done )
		condor_post_external( cream_wsdl OFF OFF )

		set (CREAM_PATCH patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/cream.patch &&
				 patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/voms2.patch)
		if (DARWIN)
		  set (CREAM_PATCH ${CREAM_PATCH} && patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/bsd.patch && patch -N -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/macos.patch )
		endif(DARWIN)

		if ( BUILDING_OPENSSL )
			set( OPENSSL_FLAG --with-openssl-prefix=${OPENSSL_INSTALL_LOC} )
		else()
			set( OPENSSL_FLAG "" )
		endif()

		if(BOOST_INSTALL_LOC)
			set ( BOOST_PREFIX --with-boost-prefix=${BOOST_INSTALL_LOC})
			set (BOOST_LIB ${BOOST_INSTALL_LOC}/lib/libboost_thread.a)
		else()
			set ( BOOST_PREFIX --with-boost-prefix=/usr)
			set (BOOST_LIB libboost_thread.a)
		endif()

		ExternalProject_Add(cream
				    DEPENDS ${VOMS_REF} ${CREAMSOAP_REF} ${BOOST_REF} ${LOG4CPP_REF} ${GRIDSITE_REF} ${CARES_REF} ${CREAM_SOAP_PLUGIN_REF} ${CREAM_WSDL_REF}
				    #-- Download Step -------
				    DOWNLOAD_DIR ${CREAM_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/org.glite.ce.cream-client-api-c-1.12.1-14.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${CREAM_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/boost.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/classads.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/glite.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/glite_ce_wsdl.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/globus.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/gsoap.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/log4cpp.m4 project/ &&
						cp ${CMAKE_CURRENT_SOURCE_DIR}/optimize.m4 project/ &&
						./bootstrap &&
						CPPFLAGS=${CPP11_FLAG} ./configure --prefix=${CREAM_INSTALL_LOC} --with-globus-prefix=${GLOBUS_INSTALL_LOC} --with-globus-thr-flavor=${GLOBUS_FLAVOR}pthr --with-globus-nothr-flavor=${GLOBUS_FLAVOR} ${OPENSSL_FLAG} --with-voms-prefix=${VOMS_INSTALL_LOC} --with-classads-prefix=${CLASSADS_INSTALL_LOC} --with-log4cpp-prefix=${LOG4CPP_INSTALL_LOC} --with-gsoap-location=${CREAM_INSTALL_LOC} --with-glite-location=${CREAM_INSTALL_LOC} --with-ce-wsdl-version=1.12.1 --with-delegation-wsdl-version=2.0.0 ${BOOST_PREFIX}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND make install && touch ${CREAM_INSTALL_LOC}/done
				)

		
		add_dependencies(cream ${CLASSADS_FOUND})

		set(CREAM_FOUND "${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_soap.a;${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_util.a;${CREAM_INSTALL_LOC}/lib/libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a;${CREAM_INSTALL_LOC}/lib/libglite_security_gss_${GLOBUS_FLAVOR}pthr.a;${LOG4CPP_INSTALL_LOC}/lib/liblog4cpp.a;${CREAM_INSTALL_LOC}/lib/libgridsite.a;${CREAM_INSTALL_LOC}/lib/libcares.a;${BOOST_LIB}")

		condor_post_external( cream include OFF )

else( NOT PROPER )

        if ( ${SYSTEM_NAME} MATCHES "rhel6" OR ${SYSTEM_NAME} MATCHES "centos6" OR ${SYSTEM_NAME} MATCHES "sl6")
            find_multiple("glite_ce_cream_client_soap;glite_ce_cream_client_util;glite_security_gsoap_plugin_2716_cxx;glite_security_gss;log4cpp;gridsite;boost_thread;boost_regex" CREAM_FOUND )
        else()
            find_multiple("glite_ce_cream_client_soap;glite_ce_cream_client_util;glite_security_gsoap_plugin_2713_cxx;glite_security_gss;log4cpp;gridsite;boost_thread;boost_regex" CREAM_FOUND )
        endif()

        if (CREAM_FOUND)
            dprint("CREAM_FOUND=${CREAM_FOUND}") 
            
            find_multiple("log4cpp" LOG4CPP)
            if (LOG4CPP)
                set (CREAM_FOUND "${CREAM_FOUND};${LOG4CPP}")
            else()
                message ( STATUS "missing cream dependencies (LOG4CPP=${LOG4CPP})" )
                unset (CREAM_FOUND)
            endif()
        endif()

endif( NOT PROPER )

else(WITH_CREAM)
	message (STATUS "external skipped (cream)")
endif(WITH_CREAM)

if (CREAM_FOUND)
        set( CREAM_FOUND ${CREAM_FOUND} PARENT_SCOPE )
        set( HAVE_EXT_CREAM ON PARENT_SCOPE )
        message ( STATUS "external configured (CREAM_FOUND=${CREAM_FOUND})" )
else(CREAM_FOUND)
        message (STATUS "external skipped (cream)")
endif(CREAM_FOUND)

