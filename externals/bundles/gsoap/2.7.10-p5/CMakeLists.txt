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

option(WITH_GSOAP "Compiling with support for GSOAP" ON)

if (WITH_GSOAP)

	if ( NOT PROPER )

		set (GSOAP_VER gsoap-2.7.10)

		if (WINDOWS)
			set (GSOAP_PATCH cp ${CMAKE_CURRENT_SOURCE_DIR}/config.WINDOWS.h gsoap/src/config.h)
			
			set (GSOAP_CONFIGURE cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.win32 gsoap/src/Makefile.win32 )
			
			set (GSOAP_MAKE set BISON_SIMPLE=${BISON_SIMPLE}\r\n
			                gmake -f Makefile.win32 -C gsoap/src )
			
			set (GSOAP_INSTALL set PACKAGE_INSTALL_DIR=${EXTERNAL_STAGE}\r\n
			                   gmake -f Makefile.win32 install -C gsoap/src )
		else(WINDOWS)
			set (GSOAP_PATCH  patch --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/install_soapcpp2_wsdl2h_aux-2.7.10-Makefile.in.patch &&
							  patch --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/fix_xlocale_insanity.patch &&
							  patch --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/nmi_prereq_hack.patch)
			set (GSOAP_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE} --disable-dependency-tracking CPPFLAGS=-I${EXTERNAL_STAGE}/include/)
			set (GSOAP_MAKE make )
			set (GSOAP_INSTALL make install )
		endif(WINDOWS)

		ExternalProject_Add( gsoap
					DEPENDS openssl
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${GSOAP_VER}
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GSOAP_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${GSOAP_VER}.tar.gz
					#--Patch step ----------
					PATCH_COMMAND cd ${GSOAP_VER} ${CMD_TERM} ${GSOAP_PATCH}
					#--Configure step ----------
				    CONFIGURE_COMMAND cd ${GSOAP_VER} ${CMD_TERM} ${GSOAP_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${GSOAP_VER} ${CMD_TERM} ${GSOAP_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd ${GSOAP_VER} ${CMD_TERM} ${GSOAP_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (NOT WINDOWS)
			set(GSOAP_FOUND "${EXTERNAL_STAGE}/lib/libgsoapssl++.a")
			set(SOAPCPP2 "${EXTERNAL_STAGE}/bin/soapcpp2")
		else(NOT WINDOWS)
			set(GSOAP_FOUND ON)
			set(GSOAP_SRC "${EXTERNAL_STAGE}/src/stdsoap2.cpp" PARENT_SCOPE )
			set(SOAPCPP2 "${EXTERNAL_STAGE}/soapcpp2.exe")
			set_property( TARGET gsoap PROPERTY FOLDER "externals" )
		endif(NOT WINDOWS)

		set ( GSOAP_SHARE "${EXTERNAL_BUILD_PREFIX}/${GSOAP_VER}/src/gsoap/${GSOAP_VER}/gsoap" )
		append_var (CONDOR_EXTERNALS gsoap)
		set(WSSEAPI "WSSEAPI-NOTFOUND")

	else( NOT PROPER )

		#find gsoap and supporting bit(s)
		find_multiple( "gsoapssl++" GSOAP_FOUND)
		find_program( SOAPCPP2 soapcpp2 HINTS /usr/bin )

		set ( SOAPCPP2 ${SOAPCPP2} )
		set ( GSOAP_SHARE "/usr/share/gsoap" )

		# so here is the crazy problem.  There is a version issue with newer 
 		# builds of gsoap which requires us to support both old and new.
 		# so 1st we check to see if there is a cpp native and see if our patch 
		# can be applied.  
		
		# if it fails the try to use the native wsseapi and updates further down will handle 
		# condor_amazon should deal with the issue.   
		find_file(WSSEAPI wsseapi.cpp HINTS ${GSOAP_SHARE}/plugin/ )

		exec_program ( cp
					   ARGS ${GSOAP_SHARE}/plugin/wsseapi.c ${EXTERNAL_STAGE}/src/wsseapi.cpp && patch -p3 ${EXTERNAL_STAGE}/src/wsseapi.cpp ${CMAKE_CURRENT_SOURCE_DIR}/wsseapi.patch
					   RETURN_VALUE WSSEAPI_PATCH )
		
		dprint("WSSEAPI_PATCH = ${WSSEAPI_PATCH}")
		if (WSSEAPI_PATCH EQUAL 0)
		  set(WSSEAPI ${EXTERNAL_STAGE}/src/wsseapi.cpp)
		  set(PRE_WSEAPPI_CPP ON PARENT_SCOPE)
		endif()

	endif( NOT PROPER )

	if (GSOAP_FOUND)

		if (NOT WINDOWS)
		
			set( GSOAP_FOUND ${GSOAP_FOUND} PARENT_SCOPE )

			# removing dependencies from condor-proper.
			command_target( cp_smdevp cp "${GSOAP_SHARE}/plugin/smdevp.c;${EXTERNAL_STAGE}/src/smdevp.cpp" "${EXTERNAL_STAGE}/src/smdevp.cpp" )
			append_var (CONDOR_EXTERNALS cp_smdevp)
			add_dependencies(cp_smdevp gsoap)

			if (${WSSEAPI} STREQUAL "WSSEAPI-NOTFOUND" )

			  command_target(cp_wsseapi cp "${GSOAP_SHARE}/plugin/wsseapi.c;${EXTERNAL_STAGE}/src/wsseapi.cpp;&&;patch;-p3;${EXTERNAL_STAGE}/src/wsseapi.cpp;${CMAKE_CURRENT_SOURCE_DIR}/wsseapi.patch" "${EXTERNAL_STAGE}/src/wsseapi.cpp")

			  set(WSSEAPI ${EXTERNAL_STAGE}/src/wsseapi.cpp)
			  set(PRE_WSEAPPI_CPP ON PARENT_SCOPE)
			  add_dependencies(cp_wsseapi gsoap)
			  append_var (CONDOR_EXTERNALS cp_wsseapi)

			endif()

			set( WSSEAPI ${WSSEAPI} PARENT_SCOPE )
			set( SMDEVP  ${EXTERNAL_STAGE}/src/smdevp.cpp PARENT_SCOPE )
		
		endif(NOT WINDOWS)
		
		set( SOAPCPP2 ${SOAPCPP2} PARENT_SCOPE )
		set( GSOAP_SHARE ${GSOAP_SHARE} PARENT_SCOPE )
		set( HAVE_EXT_GSOAP ON PARENT_SCOPE )

		message ( STATUS "external configured (GSOAP_FOUND=${GSOAP_FOUND})" )

	endif (GSOAP_FOUND)

else(WITH_GSOAP)

	message (STATUS "external skipped (gsoap)")

endif(WITH_GSOAP)
