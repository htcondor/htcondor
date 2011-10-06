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

if (HAVE_EXT_GLOBUS AND HAVE_LDAP_H AND LDAP_FOUND )

	add_definitions(-DLDAP_DEPRECATED)

	condor_glob( HeaderFiles SourceFiles "" )

	condor_exe( nordugrid_gahp
				"${HeaderFiles};${SourceFiles}"
				${C_SBIN}
				"${GLOBUS_GRID_UNIVERSE_NORDUGRID};${GLOBUS_GRID_UNIVERSE_COMMON};${GLOBUS_FOUND};${OPENSSL_FOUND};${LDAP_FOUND}"
				OFF )

	# Check nordugrid_gahp's shared library dependencies and copy a
	# specific subset of those libraries into <release>/lib/condor
	if ( LINUX AND NOT CONDOR_PACKAGE_BUILD )
		set( SRC_DIR ${CMAKE_SOURCE_DIR} )
		set( BIN_DIR ${CMAKE_BINARY_DIR} )
		install( CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/condor)" )
		install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${BIN_DIR}/src/nordugrid_gahp/nordugrid_gahp ${SRC_DIR}/src/condor_scripts/syslib-patterns
			 COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
			 )" )
		# This is required for opensuse, where the system's libssl and
		# libcrypto files don't have owner write permission.
		install( CODE "execute_process(COMMAND chmod -R u+w ${CMAKE_INSTALL_PREFIX}/lib/condor/)" )
	endif()

else()

	message( STATUS "NOTE: *nordugrid* targets will not be built HAVE_EXT_GLOBUS=${HAVE_EXT_GLOBUS} HAVE_LDAP_H=${HAVE_LDAP_H}" )

endif ()
