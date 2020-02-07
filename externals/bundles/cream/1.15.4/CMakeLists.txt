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

# Sources for some of the source tarballs:
#   http://mirror.batlab.org/pub/linux/epel/6/SRPMS/canl-c-2.1.6-1.el6.src.rpm
#   http://mirror.batlab.org/pub/linux/epel/6/SRPMS/gridsite-2.2.6-1.el6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/3/sl6/SRPMS/updates/glite-lbjp-common-gss-3.2.16-1.el6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/3/sl6/SRPMS/updates/glite-lbjp-common-gsoap-plugin-3.2.12-1.el6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/3/sl6/SRPMS/base/glite-ce-wsdl-1.15.1-1.sl6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/3/sl6/SRPMS/updates/glite-ce-cream-client-api-c-1.15.4-2.el6.src.rpm

include (FindOpenSSL)
if (LINUX AND "${OPENSSL_VERSION}" VERSION_LESS "1.1.0")
	option( WITH_CREAM "Compiling with support for CREAM" ON )
endif()

if ( WITH_CREAM )

	if ( NOT PROPER )

		if (NOT AUTOCONF OR NOT AUTOMAKE OR NOT LIBTOOLIZE)
			message(FATAL_ERROR "Can't find autoconf, automake, or libtoolize, needed to build cream external!")
		endif()

		if ( NOT HAVE_LIBCARES AND NOT HAVE_LIBCARES19 )
			message(FATAL_ERROR "Can't find libcares, needed by the cream external!")
		endif()

#########################################
		condor_pre_external( CREAMSOAP gsoap_2.7.16-p1 "lib;include" "done")
		ExternalProject_Add(creamsoap
			#-- Download Step ----------
			DOWNLOAD_DIR ${CREAMSOAP_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/gsoap_2.7.16.zip
			#--Patch step ----------
			PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gsoap_cast.patch &&
				patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gsoap_ipv6.patch
			#--Configure step ----------
			CONFIGURE_COMMAND CFLAGS=-DWITH_IPV6 ./configure --prefix=${CREAMSOAP_INSTALL_LOC}
			#--Build Step ----------
			BUILD_COMMAND make
			BUILD_IN_SOURCE 1
			#--install Ste ----------
			INSTALL_DIR ${CREAMSOAP_INSTALL_LOC}
			INSTALL_COMMAND make install &&
				touch ${CREAMSOAP_INSTALL_LOC}/done )

		condor_post_external( creamsoap OFF OFF "${CREAMSOAP_INSTALL_LOC}/lib/libgsoapssl++.a" )

#########################################

		condor_pre_external( LOG4CPP log4cpp-1.0-3 "lib;include" "lib/liblog4cpp.a")
		ExternalProject_Add(log4cpp
			#-- Download Step ----------
			DOWNLOAD_DIR ${LOG4CPP_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/log4cpp-1.0-3.tar.gz
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
		condor_post_external( log4cpp include OFF "${LOG4CPP_INSTALL_LOC}/lib/liblog4cpp.a" )

######################################### cream/gss packages.

		if ( HAVE_LIBCARES19 )
			set( CANL_C_FLAGS CFLAGS=-I/usr/include/c-ares19 LIBCARES_LIBS=-lcares19 )
		endif()

		condor_pre_external( CANL_C canl-c-2.1.6 "lib;include" "done")
		ExternalProject_Add(canl_c
			#-- Download Step ----------
			DOWNLOAD_DIR ${CANL_C_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/canl-c-2.1.6.tar.gz
			#--Patch step ----------
			PATCH_COMMAND patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/canl_install.patch
			#--Configure step ----------
			CONFIGURE_COMMAND ./configure --root=/ --prefix=${CANL_C_INSTALL_LOC} --libdir=lib
			#--Build Step ----------
			BUILD_COMMAND ${CANL_C_FLAGS} make libcanl_c.la
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${CANL_C_INSTALL_LOC}
			INSTALL_COMMAND make install &&
				touch ${CANL_C_INSTALL_LOC}/done )
		condor_post_external( canl_c OFF OFF "${CANL_C_INSTALL_LOC}/lib/libcanl_c.a" )

#########################################

		condor_pre_external( GRIDSITE gridsite-2.2.6 "lib;include" "done")
		ExternalProject_Add(gridsite
			DEPENDS ${CANL_C_REF}
			#-- Download Step ----------
			DOWNLOAD_DIR ${GRIDSITE_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/gridsite-core_R_2_2_6.tar.gz
			#--Patch step ----------
			PATCH_COMMAND sed -i -e "s|^CANL_C_CFLAGS.*|CANL_C_CFLAGS=-I${CANL_C_INSTALL_LOC}/include|" -e "s|^CANL_C_LIBS.*|CANL_C_LIBS=-L${CANL_C_INSTALL_LOC}/lib -lcanl_c|" src/Makefile
			#--Configure step ----------
			CONFIGURE_COMMAND echo "no config"
			#--Build Step ----------
			BUILD_COMMAND cd src &&
				make libgridsite.a
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${GRIDSITE_INSTALL_LOC}
			INSTALL_COMMAND cd src &&
				libtool --mode=install install libgridsite.la ${GRIDSITE_INSTALL_LOC}/lib &&
				cp -f ../interface/gridsite-gacl.h ../interface/gridsite.h ${GRIDSITE_INSTALL_LOC}/include &&
				touch ${GRIDSITE_INSTALL_LOC}/done )
		condor_post_external( gridsite OFF OFF  "${GRIDSITE_INSTALL_LOC}/lib/libgridsite.a" )

#########################################

		if ( HAVE_LIBCARES19 )
			set( GLITE_SECURITY_FLAGS LIBCARES_CFLAGS=-I/usr/include/c-ares19 LIBCARES_LIBS=-lcares19 )
		endif()

		condor_pre_external( GLITE_SECURITY glite-lbjp-common-gss-3.2.16 "lib;include" "done")
		ExternalProject_Add(glite_security
			DEPENDS ${GLOBUS_REF}
			#-- Download Step ----------
			DOWNLOAD_DIR ${GLITE_SECURITY_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/glite-lbjp-common-gss-3.2.16.tar.gz
			#--Configure step ----------
			CONFIGURE_COMMAND ./configure --with-globus=${GLOBUS_INSTALL_LOC}
			#--Build Step ----------
			BUILD_COMMAND ${GLITE_SECURITY_FLAGS} CFLAGS=-I${GLOBUS_INSTALL_LOC}/include/globus make libglite_security_gss.a
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${GLITE_SECURITY_INSTALL_LOC}
			INSTALL_COMMAND mkdir -p ${GLITE_SECURITY_INSTALL_LOC}/include/glite/security &&
				cp interface/glite_gss.h ${GLITE_SECURITY_INSTALL_LOC}/include/glite/security &&
				cp libglite_security_gss.a ${GLITE_SECURITY_INSTALL_LOC}/lib &&
				touch ${GLITE_SECURITY_INSTALL_LOC}/done )
		condor_post_external( glite_security include OFF "${GLITE_SECURITY_INSTALL_LOC}/lib/libglite_security_gss.a" )

#########################################

		# build gsoap plugin.
		condor_pre_external( CREAM_SOAP_PLUGIN glite-lbjp-common-gsoap-plugin-3.2.12 "lib;include" "done")
		ExternalProject_Add(cream_soap_plugin
			DEPENDS ${CREAMSOAP_REF} ${GLITE_SECURITY_REF}
			#-- Download Step ----------
			DOWNLOAD_DIR ${CREAM_SOAP_PLUGIN_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/glite-lbjp-common-gsoap-plugin-3.2.12.tar.gz
			#--Configure step ----------
			CONFIGURE_COMMAND ./configure --with-globus=${GLOBUS_INSTALL_LOC} --with-gsoap=${CREAMSOAP_INSTALL_LOC}
			#--Build Step ----------
			BUILD_COMMAND CFLAGS=-I${GLITE_SECURITY_INSTALL_LOC}/include make link-gsoap soap_version.h glite_gsplugin.cxx.thr.o &&
				ar crv libglite_security_gsoap_plugin_cxx.a glite_gsplugin.cxx.thr.o &&
				ranlib libglite_security_gsoap_plugin_cxx.a
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib
			INSTALL_COMMAND mkdir -p ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include/glite/security &&
				cp libglite_security_gsoap_plugin_cxx.a ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib &&
				cp interface/glite_gsplugin.h interface/glite_gsplugin-int.h ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include/glite/security &&
				touch ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/done )
		condor_post_external( cream_soap_plugin include OFF "${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib/libglite_security_gsoap_plugin_cxx.a" )

		condor_pre_external( CREAM_WSDL glite-ce-wsdl-1.15.1 "lib;include" "done")
		ExternalProject_Add(cream_wsdl
			#-- Download Step ----------
			DOWNLOAD_DIR ${CREAM_WSDL_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/glite-ce-wsdl-1.15.1.tar.gz
			#--Configure step ----------
			CONFIGURE_COMMAND echo "nothing to configure"
			#--Build Step ----------
			BUILD_COMMAND echo "nothing to build"
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${CREAM_WSDL_INSTALL_LOC}
			INSTALL_COMMAND rm -rf ${CREAM_WSDL_INSTALL_LOC}/share/wsdl &&
				mkdir -p ${CREAM_WSDL_INSTALL_LOC}/share/wsdl &&
				cp -r CREAM ${CREAM_WSDL_INSTALL_LOC}/share/wsdl/cream-ce &&
				cp -r ES ${CREAM_WSDL_INSTALL_LOC}/share/wsdl/cream-ce/es &&
				touch ${CREAM_WSDL_INSTALL_LOC}/done )
		condor_post_external( cream_wsdl OFF OFF "" )

#########################################

		# build cream c api library
		condor_pre_external( CREAM glite-ce-cream-client-api-c-1.15.4-p1 "lib;include;include/glite/security;opt" "done")

        if(BOOST_INSTALL_LOC)
            set (BOOST_INCLUDE_PATH ${BOOST_INSTALL_LOC}/include)
            set (BOOST_LIB ${BOOST_INSTALL_LOC}/lib/libboost_thread.a ${BOOST_INSTALL_LOC}/lib/libboost_filesystem.a ${BOOST_INSTALL_LOC}/lib/libboost_system.a)
        else()
            set (BOOST_INCLUDE_PATH "/usr/include")
            set (BOOST_LIB libboost_thread.a libboost_filesystem.a libboost_system.a)
        endif()

        if ( ${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7")
            set (BOOST_LIB ${BOOST_LD}/libboost_thread.a ${BOOST_LD}/libboost_filesystem.a ${BOOST_LD}/libboost_system.a)
        endif()


		set (CREAM_PATCH patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/cream.patch -p1)
		if ( PREFER_CPP11 )
			set (CREAM_PATCH ${CREAM_PATCH} && patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/cream_cpp11.patch -p1)
		endif()

		ExternalProject_Add(cream
			DEPENDS ${VOMS_REF} ${CREAMSOAP_REF} ${BOOST_REF} ${LOG4CPP_REF} ${GRIDSITE_REF} ${CREAM_SOAP_PLUGIN_REF} ${CREAM_WSDL_REF}
			#-- Download Step -------
			DOWNLOAD_DIR ${CREAM_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/glite-ce-cream-client-api-c-1.15.4.tar.gz
			#--Patch step ----------
			PATCH_COMMAND ${CREAM_PATCH}
			#--Configure step ----------
			CONFIGURE_COMMAND PATH=${CREAMSOAP_INSTALL_LOC}/bin:$ENV{PATH} ${CMAKE_COMMAND} 
				-DCMAKE_INSTALL_PREFIX:string=${CREAM_INSTALL_LOC} 
				-DWSDL_PATH:string=${CREAM_WSDL_INSTALL_LOC}/share/wsdl/cream-ce 
				-DGSOAP_INCLUDE_DIRS:string=${CREAMSOAP_INSTALL_LOC}/include 
				-DLOG4CPP_INCLUDE_DIRS:string=${LOG4CPP_INSTALL_LOC}/include 
				-DVOMS_INCLUDE_DIRS:string=${VOMS_INSTALL_LOC}/include 
				-DGRIDSITE_OPENSSL_INCLUDE_DIRS:string=${GRIDSITE_INSTALL_LOC}/include 
				-DCLASSAD_INCLUDE_DIRS:string=$<TARGET_FILE_DIR:classads>/../include
				-DGLOBUS_GSSAPI_GSI_INCLUDE_DIRS:string=${GLOBUS_INSTALL_LOC}/include/globus 
				-DGLITE_GSS_INCLUDE_DIRS:string=${GLITE_SECURITY_INSTALL_LOC}/include 
				-DGLITE_PLUGIN_INCLUDE_DIRS:string=${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include 
                -DBoost_INCLUDE_DIRS:string=${BOOST_INCLUDE_PATH} .
			#--Build Step ----------
			BUILD_COMMAND PATH=${CREAMSOAP_INSTALL_LOC}/bin:$ENV{PATH} make
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${CREAM_INSTALL_LOC}
			INSTALL_COMMAND ln -s -f . ${CREAM_INSTALL_LOC}/usr &&
				make install && touch ${CREAM_INSTALL_LOC}/done
				)

		add_dependencies(cream ${CLASSADS_FOUND})

		set(CREAM_FOUND "${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_soap.a;${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_util.a;${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib/libglite_security_gsoap_plugin_cxx.a;${GLITE_SECURITY_INSTALL_LOC}/lib/libglite_security_gss.a;${LOG4CPP_INSTALL_LOC}/lib/liblog4cpp.a;${GRIDSITE_INSTALL_LOC}/lib/libgridsite.a;${CANL_C_INSTALL_LOC}/lib/libcanl_c.a;${CREAMSOAP_INSTALL_LOC}/lib/libgsoapssl++.a;${HAVE_LIBCARES};${LIBXML2_FOUND};${BOOST_LIB}")

		condor_post_external( cream include OFF "${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_soap.a;${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_util.a" )

	else( NOT PROPER )

		if ( ${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7")
			find_multiple("glite_ce_cream_client_soap;glite_ce_cream_client_util;glite_security_gsoap_plugin_2816_cxx;glite_security_gss;gsoapssl++;globus_gssapi_gsi;globus_common;log4cpp;gridsite;boost_thread;boost_regex" CREAM_FOUND )
		elseif ( ${SYSTEM_NAME} MATCHES "rhel6" OR ${SYSTEM_NAME} MATCHES "centos6" OR ${SYSTEM_NAME} MATCHES "sl6")
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

