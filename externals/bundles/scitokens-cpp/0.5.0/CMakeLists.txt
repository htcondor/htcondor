 ###############################################################
 #
 # Copyright 2019, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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

if (UNIX)
    option(WITH_SCITOKENS "Compiling with support for SciTokens" ON)
endif(UNIX)

if (WITH_SCITOKENS)

	if ( NOT PROPER OR WANT_SCITOKENS_EXTERNAL )

		# On macOS, force the SciTokens build to use the OS-supplied
		# SSL library, ignoring any from MacPorts (which CMake likes
		# to use by default).
		if ( APPLE )
			set ( CONFIGURE_ARGS -DLIBCRYPTO_INCLUDE_DIRS=${DARWIN_OPENSSL_INCLUDE} -DLIBCRYPTO_LIBRARIES=${DARWIN_OPENSSL_LIB_DIR}/libcrypto.dylib )
		endif()

		condor_pre_external( SCITOKENS scitokens-0.5.0-p1 "lib;include" "done" )

		ExternalProject_Add(scitokens
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${SCITOKENS_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/scitokens-cpp-0.5.0.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/macos-openssl.patch &&
					patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/sqlite3-leak.patch
					#--Configure step ----------
				    CONFIGURE_COMMAND cmake ${CONFIGURE_ARGS} -DJWT_CPP_DIR=${CMAKE_SOURCE_DIR}/src/condor_io/ -DCMAKE_INSTALL_PREFIX=${SCITOKENS_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${SCITOKENS_INSTALL_LOC}
				    INSTALL_COMMAND make install && touch ${SCITOKENS_INSTALL_LOC}/done)

		# Set the target dependencies which the rest of condor depends on.
		if (APPLE)
			set( SCITOKENS_SO "libSciTokens.0.dylib" )
			set (SCITOKENS_FOUND "${SCITOKENS_INSTALL_LOC}/lib/libSciTokens.dylib")
		else(APPLE)
			set( SCITOKENS_SO "libSciTokens.so.0" )
			set (SCITOKENS_FOUND "${SCITOKENS_INSTALL_LOC}/lib/libSciTokens.so")
		endif(APPLE)

		condor_post_external( scitokens include OFF "${SCITOKENS_FOUND}" )

		if ( LINUX )
			# Install the SciTokens library manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${SCITOKENS_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
				COMMAND xargs -isrc cp -a --remove-destination src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libSciTokens.so*
					COMMAND xargs strip)" )
			endif()
		endif()
		if ( APPLE )
			# Install the SciTokens library manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${SCITOKENS_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
				COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )

			# Fix up the share library dependencies
			install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libSciTokens*.dylib
				 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )

			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libSciTokens*.dylib
					COMMAND xargs strip -x)" )
			endif()
		endif()

	else(NOT PROPER)

		find_path( SCITOKENS_INCLUDE_DIRS NAMES "scitokens/scitokens.h"
			PATHS $ENV{SCITOKENS_DIR}/include ${SCITOKENS_DIR}/include )
		find_library( SCITOKENS_FOUND "SciTokens"
			HINTS $ENV{SCITOKENS_DIR} ${SCITOKENS_DIR}
			PATH_SUFFIXES lib )
		if (LINUX AND SCITOKENS_FOUND )
			find_so_name( SCITOKENS_SO ${SCITOKENS_FOUND} )
		endif()

	endif(NOT PROPER OR WANT_SCITOKENS_EXTERNAL)

	if ( SCITOKENS_FOUND )
		message( STATUS "external configured (SCITOKENS_FOUND=${SCITOKENS_FOUND})" )
	else()
		message( FATAL_ERROR "SciTokens enabled but supporting library (libSciTokens) not found." )
	endif()
	if( DLOPEN_SECURITY_LIBS )
		set( SCITOKENS_FOUND "" CACHE STRING "SciTokens Found" FORCE )
	endif()

	set( HAVE_EXT_SCITOKENS ON PARENT_SCOPE )
	set( SCITOKENS_FOUND ${SCITOKENS_FOUND} PARENT_SCOPE )
	set( SCITOKENS_INCLUDE_DIRS ${SCITOKENS_INCLUDE_DIRS} PARENT_SCOPE )
	set( LIBSCITOKENS_SO ${LIBSCITOKENS_SO} PARENT_SCOPE )

else(WITH_SCITOKENS)

	message (STATUS "external skipped (SciTokens)")

endif(WITH_SCITOKENS)
