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
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/2/sl6/SRPMS/base/glite-lbjp-common-gss-3.1.3-2.el6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/2/sl6/SRPMS/base/glite-lbjp-common-gsoap-plugin-3.1.2-2.el6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/2/sl6/SRPMS/base/glite-ce-wsdl-1.14.0-4.sl6.src.rpm
#   http://emisoft.web.cern.ch/emisoft/dist/EMI/2/sl6/SRPMS/base/glite-ce-cream-client-api-c-1.14.0-4.sl6.src.rpm

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
		condor_pre_external( CREAM glite-ce-cream-client-api-c-1.14.0-p1 "lib;include;include/glite/security;opt" "done")

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

		condor_post_external( creamsoap OFF OFF )

#########################################

		condor_pre_external( CARES c-ares-1.3.0-p10 "lib;include" "done")
		ExternalProject_Add(cares
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CARES_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/c-ares-1.3.0.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${CARES_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CARES_INSTALL_LOC}
				    INSTALL_COMMAND make
				    install && touch ${CARES_INSTALL_LOC}/done )
		condor_post_external( cares OFF OFF )

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
		condor_post_external( log4cpp include OFF )

######################################### cream/gss packages.

		condor_pre_external( GRIDSITE gridsite-1.6.0-p10 "lib;include" "done")
		ExternalProject_Add(gridsite
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${GRIDSITE_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/gridsite-1.6.0.src.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gridsite_extern.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND export "MYCFLAGS=-I. -I../interface" &&
					  cd src &&
					  make libgridsite.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND cd src &&
				    cp -f libgridsite.a ${GRIDSITE_INSTALL_LOC}/lib &&
				    cp -f ../interface/gridsite-gacl.h ../interface/gridsite.h ${GRIDSITE_INSTALL_LOC}/include &&
				    touch ${GRIDSITE_INSTALL_LOC}/done )
		condor_post_external( gridsite OFF OFF )

#########################################

		condor_pre_external( GLITE_SECURITY glite-lbjp-common-gss-3.1.3-p1 "lib;include" "done")
		ExternalProject_Add(glite_security
				    DEPENDS ${GLOBUS_REF} ${GRIDSITE_REF} ${CARES_REF}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${GLITE_SECURITY_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/glite-lbjp-common-gss-3.1.3-2.src.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/gss_import.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND cd src &&
					  gcc -g -O0 -W -Wall -D_GNU_SOURCE
					  -Wno-unused-parameter -I../interface
					  -I${CARES_INSTALL_LOC}/include -I${GLOBUS_INSTALL_LOC}/include/globus -o glite_gss.thr.o -c glite_gss.c && 
					  ar crv libglite_security_gss.a glite_gss.thr.o &&
					  ranlib libglite_security_gss.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${GLITE_SECURITY_INSTALL_LOC}
				    INSTALL_COMMAND cd src &&
					  mkdir -p ${GLITE_SECURITY_INSTALL_LOC}/include/glite/security &&
					  cp ../interface/glite_gss.h ${GLITE_SECURITY_INSTALL_LOC}/include/glite/security &&
					  cp libglite_security_gss.a ${GLITE_SECURITY_INSTALL_LOC}/lib &&
					  touch ${GLITE_SECURITY_INSTALL_LOC}/done )
		condor_post_external( glite_security OFF OFF )

#########################################

		# build gsoap plugin.
		set (CSP_CC_FLAGS -g -O0 -W -Wall -Wno-unused-parameter -I. -I../interface -DWITH_NONAMESPACES -I${CREAMSOAP_INSTALL_LOC}/include -I${GLITE_SECURITY_INSTALL_LOC}/include -D_GNU_SOURCE)
		condor_pre_external( CREAM_SOAP_PLUGIN glite-lbjp-common-gsoap-plugin-3.1.2-p1 "lib;include" "done")
		ExternalProject_Add(cream_soap_plugin
					DEPENDS ${CREAMSOAP_REF} ${GLITE_SECURITY_REF}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CREAM_SOAP_PLUGIN_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/glite-lbjp-common-gsoap-plugin-3.1.2-2.src.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd src &&
					${CMAKE_CURRENT_SOURCE_DIR}/cream_soap_plugin_patch.sh ${CREAMSOAP_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND cd src &&
						g++ ${CSP_CC_FLAGS} -o glite_gsplugin.cxx.thr.o -c glite_gsplugin.c &&
						ar crv libglite_security_gsoap_plugin_cxx.a glite_gsplugin.cxx.thr.o &&
						ranlib libglite_security_gsoap_plugin_cxx.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib
				    INSTALL_COMMAND  cd src &&
						mkdir -p ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include/glite/security &&
						cp libglite_security_gsoap_plugin_cxx.a ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib &&
						cp ../interface/glite_gsplugin.h ../interface/glite_gsplugin-int.h ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include/glite/security &&
						touch ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/done )
		condor_post_external( cream_soap_plugin OFF OFF )

		condor_pre_external( CREAM_WSDL glite-ce-wsdl-1.14.0-p1 "lib;include" "done")
		ExternalProject_Add(cream_wsdl
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${CREAM_WSDL_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/glite-ce-wsdl-1.14.0-4.sl6.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "nothing to do"
				    #--Build Step ----------
				    BUILD_COMMAND project/install.sh ${CREAM_WSDL_INSTALL_LOC}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_WSDL_INSTALL_LOC}
				    #INSTALL_COMMAND ln -fs share/wsdl ${CREAM_WSDL_INSTALL_LOC}/interface && touch ${CREAM_WSDL_INSTALL_LOC}/done )
				    INSTALL_COMMAND touch ${CREAM_WSDL_INSTALL_LOC}/done )
		condor_post_external( cream_wsdl OFF OFF )

		set (CREAM_PATCH ln -f -s lib ${CREAM_INSTALL_LOC}/lib64 &&
			mkdir -p ${CREAM_INSTALL_LOC}/include/glite/security &&
			ln -f -s ${GLITE_SECURITY_INSTALL_LOC}/include/glite/security/glite_gss.h ${CREAM_INSTALL_LOC}/include/glite/security/ &&
			ln -f -s ${CREAM_SOAP_PLUGIN_INSTALL_LOC}/include/glite/security/glite_gsplugin.h ${CREAM_INSTALL_LOC}/include/glite/security/ &&
			ln -f -s ${CARES_INSTALL_LOC}/include/ares.h ${CREAM_INSTALL_LOC}/include/ &&
			ln -f -s ${CARES_INSTALL_LOC}/include/ares_version.h ${CREAM_INSTALL_LOC}/include/ &&
			ln -f -s ${CREAMSOAP_INSTALL_LOC}/include/stdsoap2.h ${CREAM_INSTALL_LOC}/include/ &&
			ln -f -s /usr/include/libxml2/libxml ${CREAM_INSTALL_LOC}/include/ &&
			mkdir ${CREAM_INSTALL_LOC}/share &&
			ln -f -s ${CREAM_WSDL_INSTALL_LOC}/share/wsdl ${CREAM_INSTALL_LOC}/share/ &&
			patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/cream.patch &&
			patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/cream_voms.patch)
		if (DARWIN)
		  set (CREAM_PATCH ${CREAM_PATCH} && patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/bsd.patch && patch -N -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/macos.patch )
		endif(DARWIN)

		if(BOOST_INSTALL_LOC)
			set ( BOOST_PREFIX --with-boost-prefix=${BOOST_INSTALL_LOC})
			set (BOOST_LIB ${BOOST_INSTALL_LOC}/lib/libboost_thread.a ${BOOST_INSTALL_LOC}/lib/libboost_filesystem.a ${BOOST_INSTALL_LOC}/lib/libboost_system.a)
		else()
			set ( BOOST_PREFIX --with-boost-prefix=/usr)
			set (BOOST_LIB libboost_thread.a libbosst_filesystem.a libboost_system.a)
		endif()

		# ./bootstrap was called before the source package was
		# created. We have to re-execute it here. Othewise, libtool
		# complains about version skew if the build machine isn't
		# RedHat6. Before that, we have to delete the .m4 files that
		# are symlinks to files that may not be installed and
		# provide our own .m4 file that includes the autocont 
		# macros provided by other glite packages.
		#
		# We have to set a bunch of *_CFLAGS and *_LIBS variables
		# for configure so that it doesn't attempt to call
		# pkg-config to find the locations of the dependent packages.
		ExternalProject_Add(cream
				    DEPENDS ${VOMS_REF} ${CREAMSOAP_REF} ${BOOST_REF} ${LOG4CPP_REF} ${GRIDSITE_REF} ${CARES_REF} ${CREAM_SOAP_PLUGIN_REF} ${CREAM_WSDL_REF}
				    #-- Download Step -------
				    DOWNLOAD_DIR ${CREAM_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/glite-ce-cream-client-api-c-1.14.0-4.sl6.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${CREAM_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND rm project/lt~obsolete.m4 project/ltoptions.m4 project/ltsugar.m4 project/ltversion.m4 && cp ${CMAKE_CURRENT_SOURCE_DIR}/myglite.m4 project/ && ./bootstrap && CFLAGS=-I${CREAM_INSTALL_LOC}/include CPPFLAGS=${CPP11_FLAG} GRIDSITE_OPENSSL_CFLAGS=-I${GRIDSITE_INSTALL_LOC}/include GRIDSITE_OPENSSL_LIBS=${GRIDSITE_INSTALL_LOC}/lib/libgridsite.a VOMS_CFLAGS=-I${VOMS_INSTALL_LOC}/include VOMS_LIBS=${VOMS_INSTALL_LOC}/lib/libvomsapi.so CLASSAD_CFLAGS=-I${CLASSADS_INSTALL_LOC}/include CLASSAD_LIBS=${CLASSADS_INSTALL_LOC}/lib/libclassad.so LD_LIBRARY_PATH=${BOOST_INSTALL_LOC}/lib LOG4CPP_CFLAGS=-I${LOG4CPP_INSTALL_LOC}/include LOG4CPP_LIBS=${LOG4CPP_INSTALL_LOC}/lib/liblog4cpp.a GSOAP_CFLAGS=-I${CREAM_INSTALL_LOC}/include GSOAP_LIBS=${CREAMSOAP_INSTALL_LOC}/lib/libgsoapssl++.a GLITE_LOCATION=${CREAM_INSTALL_LOC} ./configure --prefix=${CREAM_INSTALL_LOC} --with-gsoap-location=${CREAMSOAP_INSTALL_LOC} ${BOOST_PREFIX} --disable-shared
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_INSTALL_LOC}
				    INSTALL_COMMAND make install && touch ${CREAM_INSTALL_LOC}/done
				)

		
		add_dependencies(cream ${CLASSADS_FOUND})

		set(CREAM_FOUND "${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_soap.a;${CREAM_INSTALL_LOC}/lib/libglite_ce_cream_client_util.a;${CREAM_SOAP_PLUGIN_INSTALL_LOC}/lib/libglite_security_gsoap_plugin_cxx.a;${GLITE_SECURITY_INSTALL_LOC}/lib/libglite_security_gss.a;${LOG4CPP_INSTALL_LOC}/lib/liblog4cpp.a;${GRIDSITE_INSTALL_LOC}/lib/libgridsite.a;${CARES_INSTALL_LOC}/lib/libcares.a;${CREAMSOAP_INSTALL_LOC}/lib/libgsoapssl++.a;${LIBXML2_FOUND};${BOOST_LIB}")

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

