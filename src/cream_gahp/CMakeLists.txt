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

if ( HAVE_EXT_CREAM )

	if ( HAVE_LIBCARES19 )
		set( CARES_LIB ${HAVE_LIBCARES19} )
	else()
		set( CARES_LIB ${HAVE_LIBCARES} )
	endif()

	# Ensure we use the version of gsoap that was used to build
	# the glite libraries.
	# This is different than the version we use for HTCondor's
	# soap support.
	if ( NOT PROPER )
		include_directories( BEFORE ${CREAMSOAP_INSTALL_LOC}/include )
	endif()

	add_definitions(-D_REENTRANT)
	# LTO fails with CREAM because they have references to the typeinfo of exceptions,
	# but the typeinfo for these are not generated as the exceptions are solely defined
	# in header files.
	# Hence, CREAM exception handling does not work; we just didn't notice until LTO was attempted.
	STRING (REGEX REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
	
	set_source_files_properties(cream_gahp_server.cpp PROPERTIES COMPILE_FLAGS -Wno-shadow)

	condor_exe( cream_gahp
		"cream_gahp_server.cpp"
		${C_SBIN}
		"${CREAM_FOUND};${VOMS_FOUND};${GLOBUS_THR_FOUND};${CLASSADS_FOUND};${LIBSSL_FOUND};${LIBCRYPTO_FOUND};${PCRE_FOUND};${CARES_LIB};${ZLIB_FOUND}"
		OFF )
	if ( NOT PROPER )
		if ( CREAMSOAP_REF )
			add_dependencies(cream_gahp ${CREAMSOAP_REF})
		endif()
		if ( GLOBUS_REF )
			add_dependencies(cream_gahp ${GLOBUS_REF})
		endif()
	endif()

	# Check cream_gahp's shared library dependencies and copy a
	# specific subset of those libraries into <release>/lib/condor
	if ( LINUX AND NOT CONDOR_PACKAGE_BUILD )
		set( SRC_DIR ${CMAKE_SOURCE_DIR} )
		set( BIN_DIR ${CMAKE_BINARY_DIR} )
		install( CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/condor)" )
		install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${BIN_DIR}/src/cream_gahp/cream_gahp ${SRC_DIR}/src/condor_scripts/syslib-patterns
			COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
			)" )
	endif()

else ( HAVE_EXT_CREAM )

	message(STATUS "NOTE: *cream* targets will not be built HAVE_EXT_CREAM=${HAVE_EXT_CREAM}")

endif ( HAVE_EXT_CREAM )

