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

if (NOT PROPER AND LINUX) #OR DARWIN
  option( WITH_CREAM "Compiling with support for CREAM" ON )
endif()
  
if ( WITH_CREAM )

		# cream is sandboxed as not to pollute the local staging directory
		# this is primarily for gsoap collision and to keep other elements
		# out of condor proper. 
		set (CREAM_STAGE ${EXTERNAL_STAGE}/opt/cream)
		file (MAKE_DIRECTORY ${CREAM_STAGE}
			${CREAM_STAGE}/lib
			${CREAM_STAGE}/include
			${CREAM_STAGE}/include/glite/security)

#################### Generic localized packages 
		set (GSOAP_VER gsoap_2.7.6b)
		ExternalProject_Add(cream_soap
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${GSOAP_VER}
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GSOAP_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${GSOAP_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd gsoap-2.7 && ./configure --prefix=${CREAM_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd gsoap-2.7 && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd gsoap-2.7 && make install && cp soapcpp2/wsdl/stlvector.h ${CREAM_STAGE}/include )

		set (CARES_VER c-ares-1.3.0)
		ExternalProject_Add(c-ares
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${CARES_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${CARES_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${CARES_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd ${CARES_VER} && ./configure --prefix=${CREAM_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${CARES_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd ${CARES_VER} && make install )
				    
		set (LOG4CPP_VER log4cpp-1.0-3)
		ExternalProject_Add(log4cpp
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${LOG4CPP_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${LOG4CPP_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${LOG4CPP_VER}.tar.gz
					#--Patch step ----------
				    PATCH_COMMAND patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/log4errors.patch &&
					  patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/log4configure.patch
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd log4cpp-1.0 && ./configure --prefix=${CREAM_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd log4cpp-1.0 && make && make check
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd log4cpp-1.0 && make install )

#################### cream/gss packages.

		# build gridsite
		set (GRIDSITE_VER gridsite-1.6.0)
		ExternalProject_Add(gridsite
					DEPENDS libxml2
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${GRIDSITE_VER}
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GRIDSITE_VER}.src.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${GRIDSITE_VER}.src.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND export MYCFLAGS='-I. -I../interface -I${EXTERNAL_STAGE}/include/libxml2' &&
					  export MYCFLAGS &&
					  cd org.gridsite.core/src &&
					  make libgridsite.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd org.gridsite.core/src && cp libgridsite.a ${CREAM_STAGE}/lib && cp ../interface/*.h ${CREAM_STAGE}/include )

		
		set (GLITE_SECURITY_VER org.glite.security.gss-2.0.1-1)
		ExternalProject_Add(glite_gss
				    DEPENDS globus gridsite c-ares
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${GLITE_SECURITY_VER}
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GLITE_SECURITY_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${GLITE_SECURITY_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "no config"
				    #--Build Step ----------
				    BUILD_COMMAND cd org.glite.security.gss/src && 
					  gcc -g -O0 -W -Wall -Wno-unused-parameter -I../interface -I${EXTERNAL_STAGE}/include -I${EXTERNAL_STAGE}/include/${GLOBUS_FLAVOR}pthr -I${CREAM_STAGE}/include -o glite_gss.thr.o -c glite_gss.c && 
					  ar crv libglite_security_gss_${GLOBUS_FLAVOR}pthr.a glite_gss.thr.o &&
					  ranlib libglite_security_gss_${GLOBUS_FLAVOR}pthr.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd org.glite.security.gss/src &&
					  cp ../interface/* ${CREAM_STAGE}/include/glite/security &&
					  cp libglite_security_gss_${GLOBUS_FLAVOR}pthr.a ${CREAM_STAGE}/lib)

		# build gsoap plugin.
		set (CSP_CC_FLAGS -g -O0 -W -Wall -Wno-unused-parameter -I. -I../interface -DWITH_NONAMESPACES -I${CREAM_STAGE}/include -D_GNU_SOURCE)
		ExternalProject_Add(cream_soap_plugin
					DEPENDS cream_soap glite_gss
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/cream_soap_plugin
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/org.glite.security.gsoap-plugin-2.0.1-3.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/org.glite.security.gsoap-plugin-2.0.1-3.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd org.glite.security.gsoap-plugin/src &&
				    	${CMAKE_CURRENT_SOURCE_DIR}/cream_soap_plugin_patch.sh
				    #--Build Step ----------
				    BUILD_COMMAND cd org.glite.security.gsoap-plugin/src &&
						gcc ${CSP_CC_FLAGS} -o glite_gsplugin.thr.o -c glite_gsplugin.c &&
						gcc ${CSP_CC_FLAGS} -o stdsoap2.thr.o -c stdsoap2.c &&
						ar crv libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a glite_gsplugin.thr.o stdsoap2.thr.o &&
						ranlib libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}/lib
				    INSTALL_COMMAND  cd org.glite.security.gsoap-plugin/src &&
						cp libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a ${CREAM_STAGE}/lib && 
						cp ../interface/*.h ${CREAM_STAGE}/include/glite/security
				)


		ExternalProject_Add(cream_wsdl
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/cream_wsdl
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/org.glite.ce.wsdl-1.12.1-14.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/org.glite.ce.wsdl-1.12.1-14.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND echo "nothing to do"
				    #--Build Step ----------
				    BUILD_COMMAND cd org.glite.ce.wsdl && 
					  project/install.sh ${CREAM_STAGE} ${CREAM_STAGE} 1.12.1 2.0.0
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${CREAM_STAGE}
				    INSTALL_COMMAND cd org.glite.ce.wsdl && ln -s share/wsdl ${CREAM_STAGE}/interface )

		set (CREAM_PATCH patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/cream.patch)
		if (DARWIN)
		  set (CREAM_PATCH ${CREAM_PATCH} && patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/bsd.patch && patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/macos.patch )
		endif(DARWIN)

		ExternalProject_Add(cream
				    DEPENDS voms cream_soap boost log4cpp gridsite c-ares cream_soap_plugin cream_wsdl
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/cream
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/org.glite.ce.cream-client-api-c-1.12.1-14.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/org.glite.ce.cream-client-api-c-1.12.1-14.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${CREAM_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND cp ${CMAKE_CURRENT_SOURCE_DIR}/*.m4 org.glite.ce.cream-client-api-c/project &&    
						cd org.glite.ce.cream-client-api-c &&
						./bootstrap &&
						./configure --prefix=${CREAM_STAGE} --with-globus-prefix=${EXTERNAL_STAGE} --with-globus-thr-flavor=${GLOBUS_FLAVOR}pthr --with-globus-nothr-flavor=${GLOBUS_FLAVOR} --with-openssl-prefix=${EXTERNAL_STAGE} --with-voms-prefix=${EXTERNAL_STAGE} --with-boost-prefix=${EXTERNAL_STAGE} --with-classads-prefix=${EXTERNAL_STAGE} --with-log4cpp-prefix=${CREAM_STAGE} --with-gsoap-location=${CREAM_STAGE} --with-glite-location=${CREAM_STAGE} --with-ce-wsdl-version=1.12.1 --with-delegation-wsdl-version=2.0.0
				    #--Build Step ----------
				    BUILD_COMMAND cd org.glite.ce.cream-client-api-c && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd org.glite.ce.cream-client-api-c &&
						make install
				)

		add_dependencies(cream classads)

	# Set the target dependencies which the rest of condor depends on.
	if(DARWIN)
		set (BOOST_LIB ${EXTERNAL_STAGE}/lib/libboost_thread.a)
	else()
		set (BOOST_LIB ${EXTERNAL_STAGE}/lib/libboost_thread.a)
	endif()

	set(CREAM_FOUND "${CREAM_STAGE}/lib/libglite_ce_cream_client_soap.a;${CREAM_STAGE}/lib/libglite_ce_cream_client_util.a;${CREAM_STAGE}/lib/libglite_security_gsoap_plugin_${GLOBUS_FLAVOR}pthr.a;${CREAM_STAGE}/lib/libglite_security_gss_${GLOBUS_FLAVOR}pthr.a;${CREAM_STAGE}/lib/liblog4cpp.a;${CREAM_STAGE}/lib/libgridsite.a;${BOOST_LIB};${CREAM_STAGE}/lib/libcares.a")

	append_var(CONDOR_EXTERNALS cream)

	message (STATUS "external configured (CREAM_FOUND=${CREAM_FOUND})")
	set( CREAM_FOUND ${CREAM_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_CREAM ON PARENT_SCOPE )
	set( CREAM_STAGE ${CREAM_STAGE} PARENT_SCOPE )

else()
	message (STATUS "external skipped (cream)")
endif()
