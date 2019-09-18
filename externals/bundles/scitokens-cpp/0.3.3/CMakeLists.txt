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
    option(WITH_SCITOKENS "Compiling with support for SciTokens" OFF)
endif(UNIX)

if (WITH_SCITOKENS)

	if ( NOT PROPER )

		condor_pre_external( SCITOKENS scitokens-0.3.3 "lib;include" "done" )

		ExternalProject_Add(scitokens
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${SCITOKENS_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/scitokens-cpp-0.3.3.tar.gz
					#--Configure step ----------
				    CONFIGURE_COMMAND cmake -DJWT_CPP_DIR=${CMAKE_SOURCE_DIR}/src/condor_io/ -DCMAKE_INSTALL_PREFIX=${SCITOKENS_INSTALL_LOC}
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
			set( SCITOKENS_SO "libSciTokens.0.dylib" )
			set (SCITOKENS_FOUND "${SCITOKENS_INSTALL_LOC}/lib/libSciTokens.so")
		endif()

		condor_post_external( scitokens include OFF "${SCITOKENS_FOUND}" )

	else(NOT PROPER)

		find_path( SCITOKENS_INCLUDE_DIRS NAMES "scitokens/scitokens.h"
			PATHS $ENV{SCITOKENS_DIR}/include ${SCITOKENS_DIR}/include )
		find_library( SCITOKENS_FOUND "SciTokens"
			HINTS $ENV{SCITOKENS_DIR} ${SCITOKENS_DIR}
			PATH_SUFFIXES lib )
		if (LINUX AND SCITOKENS_FOUND )
			find_so_name( SCITOKENS_SO ${SCITOKENS_FOUND} )
		endif()

	endif(NOT PROPER)

	if ( SCITOKENS_FOUND )
		message( STATUS "external configured (SCITOKENS_FOUND=${SCITOKENS_FOUND})" )
	else()
		message( FATAL_ERROR "SciTokens enabled but supporting library (libSciTokens) not found." )
	endif()
	if( DLOPEN_SECURITY_LIBS )
		set( SCITOKENS_FOUND "" )
	endif()

	set( HAVE_EXT_SCITOKENS ON PARENT_SCOPE )
	set( SCITOKENS_FOUND ${SCITOKENS_FOUND} PARENT_SCOPE )
	set( SCITOKENS_INCLUDE_DIRS ${SCITOKENS_INCLUDE_DIRS} PARENT_SCOPE )
	set( LIBSCITOKENS_SO ${LIBSCITOKENS_SO} PARENT_SCOPE )

else(WITH_SCITOKENS)

	message (STATUS "external skipped (SciTokens)")

endif(WITH_SCITOKENS)
