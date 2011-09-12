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


if (LINUX OR DARWIN)
  option( WITH_VOMS "Compiling with support for VOMS" ON )
endif(LINUX OR DARWIN)

if ( WITH_VOMS )

	if (NOT PROPER)
		if(${SYS_ARCH} MATCHES "X86_64" AND LINUX)
			set(VOMS_LIB lib64)
		else()
			set(VOMS_LIB lib)
		endif()

		if ( BUILDING_OPENSSL )
			set (VOMS_DEPENDS ${OPENSSL_REF} ${GLOBUS_REF})
			set (VOMS_PATCH patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/makefile.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/private.patch &&
								${CMAKE_CURRENT_SOURCE_DIR}/voms_file_patch.sh )
			set (VOMS_OPENSSL_FLAG --with-openssl-prefix=${OPENSSL_INSTALL_LOC})
		else()
			set (VOMS_DEPENDS ${GLOBUS_REF})
			set (VOMS_PATCH patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure_openssl.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/makefile.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/private.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/openssl_1_0.patch &&
								${CMAKE_CURRENT_SOURCE_DIR}/voms_file_patch.sh )
			if ( ${SYSTEM_NAME} MATCHES "rhel3" )
				set (VOMS_OPENSSL_FLAG --with-openssl-prefix=/usr/kerberos)
			else()
				set (VOMS_OPENSSL_FLAG "")
			endif()
		endif()

		condor_pre_external( VOMS voms-1.9.10_4-p2 "include;${VOMS_LIB}" "done")

		ExternalProject_Add(voms
							DEPENDS ${VOMS_DEPENDS}
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/voms-1.9.10_4.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${VOMS_STAGE}/dl
							URL http://parrot.cs.wisc.edu/externals/voms-1.9.10_4.tar.gz
							#--Patch step ----------
							PATCH_COMMAND ${VOMS_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND cd org.glite.security.voms &&
								./configure
								--prefix=${VOMS_INSTALL_LOC}
								--with-api-only
								${VOMS_OPENSSL_FLAG}
								--with-globus-prefix=${GLOBUS_INSTALL_LOC}
								--with-globus-flavor=${GLOBUS_FLAVOR} --disable-java --disable-docs --disable-glite
							#--Build Step ----------
							BUILD_COMMAND cd org.glite.security.voms && make
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${VOMS_INSTALL_LOC}
							INSTALL_COMMAND cd org.glite.security.voms && make install && touch ${VOMS_INSTALL_LOC}/done
				)

		if ( LINUX )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_${GLOBUS_FLAVOR}.so" )
		else()
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_${GLOBUS_FLAVOR}.a" )
		endif()

		condor_post_external( voms include/glite/security OFF )

		if ( LINUX )
			file( GLOB Libs "${VOMS_INSTALL_LOC}/${VOMS_LIB}/*.so*")
			install( FILES ${Libs} DESTINATION "${EXTERNALS_LIB}" PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_READ WORLD_EXECUTE )
			if ( CONDOR_STRIP_PACKAGES AND NOT CONDOR_PACKAGE_BUILD )
				file( GLOB StripFiles "${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB}/libvoms*.so*" )
				install( CODE "execute_process(COMMAND strip ${StripFiles})" )
			endif()
		endif()

	else(NOT PROPER)

		find_multiple( "vomsapi" VOMS_FOUND)

	endif(NOT PROPER)

	if (VOMS_FOUND)
		message ( STATUS "external configured (VOMS_FOUND=${VOMS_FOUND})" )
		set( HAVE_EXT_VOMS ON PARENT_SCOPE )
		set( VOMS_FOUND ${VOMS_FOUND} PARENT_SCOPE )
	endif()

endif()

if(NOT VOMS_FOUND)
	message ( STATUS "external skipped (voms)" )
endif()
