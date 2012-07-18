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
		elseif( DARWIN )
			set (VOMS_DEPENDS ${GLOBUS_REF})
			set (VOMS_PATCH patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure_openssl.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/makefile.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/private.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/openssl_1_0.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/globus52.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/replib.patch &&
								cp ${CMAKE_CURRENT_SOURCE_DIR}/dummy.c org.glite.security.voms/src/replib &&
								${CMAKE_CURRENT_SOURCE_DIR}/voms_file_patch.sh )
			set (VOMS_OPENSSL_FLAG "")
		else()
			set (VOMS_DEPENDS ${GLOBUS_REF})
			set (VOMS_PATCH patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure_openssl.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/makefile.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/private.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/openssl_1_0.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/globus52.patch &&
								${CMAKE_CURRENT_SOURCE_DIR}/voms_file_patch.sh )
			if ( ${SYSTEM_NAME} MATCHES "rhel3" )
				set (VOMS_OPENSSL_FLAG --with-openssl-prefix=/usr/kerberos)
			else()
				set (VOMS_OPENSSL_FLAG "")
			endif()
		endif()

		condor_pre_external( VOMS voms-1.9.10_4-p3 "include;${VOMS_LIB}" "done")

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
								--with-globus-flavor=${GLOBUS_FLAVOR}pthr --disable-java --disable-docs --disable-glite
							#--Build Step ----------
							BUILD_COMMAND cd org.glite.security.voms && make
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${VOMS_INSTALL_LOC}
							INSTALL_COMMAND cd org.glite.security.voms && make install && touch ${VOMS_INSTALL_LOC}/done
				)

		if ( LINUX )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_${GLOBUS_FLAVOR}pthr.so" )
		elseif ( DARWIN )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_${GLOBUS_FLAVOR}pthr.dylib" )
		else()
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_${GLOBUS_FLAVOR}pthr.a" )
		endif()

		condor_post_external( voms include/glite/security OFF )

		if ( LINUX )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/${VOMS_LIB} -maxdepth 1 -name *.so*
				COMMAND xargs -isrc cp -a src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.so*
					COMMAND xargs strip)" )
			endif()
			# Check libvomsapi's shared library dependencies
			# and copy a specific subset of those libraries
			# into <release>/lib/condor
			# This is to pick up libexpat, which libvomsapi
			# depends on, but none of the binaries that use it do.
			if ( NOT CONDOR_PACKAGE_BUILD )
				set( SRC_DIR ${CMAKE_SOURCE_DIR} )
				install( CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/condor)" )
				install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${VOMS_INSTALL_LOC}/${VOMS_LIB}/libvomsapi_*.so ${SRC_DIR}/src/condor_scripts/syslib-patterns
					 COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
			 		 )" )
			endif()
		endif()
		if ( DARWIN )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/${VOMS_LIB} -maxdepth 1 -name *.dylib*
				COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )

				# Fix up the share library dependencies
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.dylib*
					 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )

			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.dylib*
					COMMAND xargs strip -x)" )
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
