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

	# this is only done in proper mode to cache local mods which are done to gsoap
	condor_pre_external( GSOAP gsoap-2.7.10-p1 "src;include" "done")

	if ( NOT PROPER )

		if (WINDOWS)
			set (GSOAP_PATCH cp ${CMAKE_CURRENT_SOURCE_DIR}/config.WINDOWS.h gsoap/src/config.h)
			
			set (GSOAP_CONFIGURE cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.win32 gsoap/src/Makefile.win32 )
			
			set (GSOAP_MAKE set BISON_SIMPLE=${BISON_SIMPLE}\r\n
			                gmake -f Makefile.win32 -C gsoap/src )
			
			set (GSOAP_INSTALL set PACKAGE_INSTALL_DIR=${GSOAP_INSTALL_LOC}\r\n
			                   gmake -f Makefile.win32 install -C gsoap/src )
		else()
			if (NOT BISON OR NOT FLEX)
				message(FATAL_ERROR "Can't find bison and flex, needed to build gsoap external!")
			endif()

			set (GSOAP_PATCH  patch -N --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/install_soapcpp2_wsdl2h_aux-2.7.10-Makefile.in.patch &&
							  patch -N --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/fix_xlocale_insanity.patch &&
							  patch -N --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/nmi_prereq_hack.patch &&
							  patch -N --binary -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/openssl_1_0.patch)
			if(DARWIN AND cxx_11)
				set (GSOAP_CONFIGURE ./configure --prefix=${GSOAP_INSTALL_LOC} --disable-dependency-tracking "CPPFLAGS=-I${OPENSSL_INSTALL_LOC}/include/ -DWITH_IPV6" "CXXFLAGS=-std=c++11 -stdlib=libc++ -DWITH_IPV6" "CFLAGS=-DWITH_IPV6" )
			else()
				set (GSOAP_CONFIGURE ./configure --prefix=${GSOAP_INSTALL_LOC} --disable-dependency-tracking "CPPFLAGS=-I${OPENSSL_INSTALL_LOC}/include/ -DWITH_IPV6" "CXXFLAGS=-DWITH_IPV6" "CFLAGS=-DWITH_IPV6" )
			endif()
			set (GSOAP_MAKE make )
			set (GSOAP_INSTALL make install )
		endif(WINDOWS)

		if ( BUILDING_OPENSSL )
			set (GSOAP_DEPENDS ${OPENSSL_REF})
		else()
			set (GSOAP_DEPENDS "")
		endif()

		message (STATUS "OPENSSL_INSTALL_LOC = ${OPENSSL_INSTALL_LOC}")

		ExternalProject_Add( gsoap
				    DEPENDS ${GSOAP_DEPENDS}
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${GSOAP_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/gsoap-2.7.10.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${GSOAP_PATCH}
					#--Configure step ----------
				    CONFIGURE_COMMAND ${GSOAP_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND ${GSOAP_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${GSOAP_INSTALL_LOC}
				    INSTALL_COMMAND ${GSOAP_INSTALL} ${CMD_TERM}
				    touch ${GSOAP_INSTALL_LOC}/done )

		# Set the target dependencies which the rest of condor depends on.
		if (NOT WINDOWS)
			set(GSOAP_FOUND "${GSOAP_INSTALL_LOC}/lib/libgsoapssl++.a")	
			set(SOAPCPP2 "${GSOAP_INSTALL_LOC}/bin/soapcpp2")
			condor_post_external( gsoap include OFF )
		else(NOT WINDOWS)
			condor_post_external( gsoap include OFF )
			# gsoap is handled differently on windows
			set(GSOAP_FOUND ON)
			set(GSOAP_SRC "${GSOAP_INSTALL_LOC}/src/stdsoap2.cpp" PARENT_SCOPE )
			set(SOAPCPP2 "${GSOAP_INSTALL_LOC}/soapcpp2.exe")
		endif(NOT WINDOWS)		
		
		set ( GSOAP_SHARE "${GSOAP_INSTALL_LOC}/share/gsoap" )
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
					   ARGS ${GSOAP_SHARE}/plugin/wsseapi.c ${GSOAP_INSTALL_LOC}/src/wsseapi.cpp && patch -N -p3 ${GSOAP_INSTALL_LOC}/src/wsseapi.cpp ${CMAKE_CURRENT_SOURCE_DIR}/wsseapi.patch
					   RETURN_VALUE WSSEAPI_PATCH )
		
		dprint("WSSEAPI_PATCH = ${WSSEAPI_PATCH}")
		if (WSSEAPI_PATCH EQUAL 0)
		  set(WSSEAPI ${GSOAP_INSTALL_LOC}/src/wsseapi.cpp)
		  set(PRE_WSEAPPI_CPP ON PARENT_SCOPE)
		endif()

	endif( NOT PROPER )

	if (GSOAP_FOUND)

		if (NOT WINDOWS)
		
			set( GSOAP_FOUND ${GSOAP_FOUND} PARENT_SCOPE )

			# removing dependencies from condor-proper.
			command_target( cp_smdevp cp "${GSOAP_SHARE}/plugin/smdevp.c;${GSOAP_INSTALL_LOC}/src/smdevp.cpp" "${GSOAP_INSTALL_LOC}/src/smdevp.cpp" )
			append_var (CONDOR_EXTERNALS cp_smdevp)
			if (GSOAP_REF)
				add_dependencies(cp_smdevp ${GSOAP_REF})
			endif()

			if (${WSSEAPI} STREQUAL "WSSEAPI-NOTFOUND" )

			  command_target(cp_wsseapi cp "${GSOAP_SHARE}/plugin/wsseapi.c;${GSOAP_INSTALL_LOC}/src/wsseapi.cpp;&&;patch;-N;-p3;${GSOAP_INSTALL_LOC}/src/wsseapi.cpp;${CMAKE_CURRENT_SOURCE_DIR}/wsseapi.patch" "${GSOAP_INSTALL_LOC}/src/wsseapi.cpp")

			  set(WSSEAPI ${GSOAP_INSTALL_LOC}/src/wsseapi.cpp)
			  set(PRE_WSEAPPI_CPP ON PARENT_SCOPE)
			  if (GSOAP_REF)
			     add_dependencies(cp_wsseapi ${GSOAP_REF})
			  endif()
			  append_var (CONDOR_EXTERNALS cp_wsseapi)

			endif()

			set( WSSEAPI ${WSSEAPI} PARENT_SCOPE )
			set( SMDEVP  ${GSOAP_INSTALL_LOC}/src/smdevp.cpp PARENT_SCOPE )
		
		endif(NOT WINDOWS)
		
		set( SOAPCPP2 ${SOAPCPP2} PARENT_SCOPE )
		set( GSOAP_SHARE ${GSOAP_SHARE} PARENT_SCOPE )
		set( HAVE_EXT_GSOAP ON PARENT_SCOPE )
		# TODO: use pkg-config to detect the correct flags on proper
		# builds
		set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DWITH_IPV6" PARENT_SCOPE )

		message ( STATUS "external configured (GSOAP_FOUND=${GSOAP_FOUND})" )

	endif (GSOAP_FOUND)

else(WITH_GSOAP)

	message (STATUS "external skipped (gsoap)")

endif(WITH_GSOAP)
