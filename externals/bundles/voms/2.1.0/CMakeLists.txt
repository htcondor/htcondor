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

# The source tarball voms-2.1.0-rc0.tar.gz was taken from the git
# repository https://github.com/italiangrid/voms.git using
# tag v2.1.0-rc0.
# Applied patch no_gsoap.patch and ran ./autogen.sh before creating tarball.

if (LINUX)
  option( WITH_VOMS "Compiling with support for VOMS" ON )
endif(LINUX)

if ( WITH_VOMS )

	if (NOT PROPER)

		if (NOT EXPAT_FOUND)
			message(FATAL_ERROR "Can't find expat, needed to build voms external!")
		endif()

		if ( APPLE )
			set ( VOMS_PATCH patch -N -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/netdb.patch )
			set ( CONFIGURE_ENV export CFLAGS=-I${DARWIN_OPENSSL_INCLUDE} && export CXXFLAGS=-I${DARWIN_OPENSSL_INCLUDE} && export OPENSSL_CFLAGS=-I${DARWIN_OPENSSL_INCLUDE} && export OPENSSL_LIBS="-L${DARWIN_OPENSSL_LIB_DIR} -lssl -lcrypto" )
		ELSE()
			set ( VOMS_PATCH patch -N -p2 < ${CMAKE_CURRENT_SOURCE_DIR}/openssl1.1.patch  && patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/voms-openssl3.patch )
			set ( CONFIGURE_ENV true )
		ENDIF()

		condor_pre_external( VOMS voms-2.1.0-rc0 "include;lib" "done")

		ExternalProject_Add(voms
			#-- Download Step ----------
			DOWNLOAD_DIR ${VOMS_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/voms-2.1.0-rc0.tar.gz
			#--Patch step ----------
			PATCH_COMMAND ${VOMS_PATCH}
			#--Configure step ----------
			CONFIGURE_COMMAND 
				${CONFIGURE_ENV} &&
				./configure
					--prefix=${VOMS_INSTALL_LOC}
					--with-api-only
					--disable-docs
			#--Build Step ----------
			BUILD_COMMAND make
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${VOMS_INSTALL_LOC}
			INSTALL_COMMAND make install && touch ${VOMS_INSTALL_LOC}/done
		)

		set( VOMS_FOUND_STATIC "${VOMS_INSTALL_LOC}/lib/libvomsapi.a" )
		if ( LINUX )
			set( LIBVOMSAPI_SO "libvomsapi.so.1" PARENT_SCOPE )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.so" )
		elseif ( APPLE )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.dylib" )
		else()
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.a" )
		endif()

		condor_post_external( voms include OFF "${VOMS_FOUND_STATIC};${VOMS_FOUND}" )

		if ( LINUX )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
				COMMAND xargs -isrc cp -a --remove-destination src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
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
				install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${VOMS_INSTALL_LOC}/lib/libvomsapi.so ${SRC_DIR}/src/condor_scripts/syslib-patterns
					 COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
			 		 )" )
			endif()
		endif()
		if ( APPLE )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
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
		find_so_name( LIBVOMSAPI_SO ${VOMS_FOUND} )
		set( LIBVOMSAPI_SO ${LIBVOMSAPI_SO} PARENT_SCOPE )

	endif(NOT PROPER)

	if (VOMS_FOUND)

		message ( STATUS "external configured (VOMS_FOUND=${VOMS_FOUND})" )
		set( HAVE_EXT_VOMS ON PARENT_SCOPE )
		set( VOMS_FOUND ${VOMS_FOUND} PARENT_SCOPE )
		set( VOMS_FOUND_STATIC ${VOMS_FOUND_STATIC} PARENT_SCOPE )
	endif()

endif()

if(NOT VOMS_FOUND)
	message ( STATUS "external skipped (voms)" )
endif()
