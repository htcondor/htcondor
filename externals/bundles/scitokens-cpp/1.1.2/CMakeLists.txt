###############################################################
#
# Copyright 2022, Condor Team, Computer Sciences Department,
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

# If we build condor to dlopen the scitokens library, we'll need a target that holds
# the header file include directory, but no library code.
add_library(SciTokens-headers INTERFACE)

if (WITH_SCITOKENS)

	if ( NOT PROPER OR WANT_SCITOKENS_EXTERNAL )

		# On macOS, force the SciTokens build to use the OS-supplied
		# SSL library, ignoring any from MacPorts (which CMake likes
		# to use by default).
		if ( APPLE )
		   get_target_property(LIBCRYPTO_INCLUDE_DIRS openssl-headers INTERFACE_INCLUDE_DIRECTORIES)
		   get_target_property(SSL_DYLIB openssl IMPORTED_LOCATION)
		   get_target_property(CRYPTO_DYLIB crypto IMPORTED_LOCATION)

		   # Make the SciTokens CMake file find our downloaded
		   # set of libressl headers and libraries
		   FetchContent_GetProperties(libressl_headers_darwin)
		   set(OPENSSL_ROOT_DIR "${libressl_headers_darwin_SOURCE_DIR}")
		endif()

		include(FetchContent)

		# This runs the wget at cmake time, runs the cmake in the tarball, and sets a bunch of scitokens_ variables
		FetchContent_declare(
			scitokens
			URL ${EXTERNALS_SOURCE_URL}/scitokens-cpp-1.1.2.tar.gz
			URL_HASH SHA256=07d33cb51a3ccd8460f2acebb15b35393aeccfc70e3554a73c9e5cffed6edb39
		)

		# Check if population has already been performed
		FetchContent_GetProperties(scitokens)
		if(NOT scitokens_POPULATED)
			# Fetch the content using previously declared details
			FetchContent_Populate(scitokens)

			# Load the scitokens cmake file, but exclude the install target, so nothing gets installed
			add_subdirectory(${scitokens_SOURCE_DIR} ${scitokens_BINARY_DIR} EXCLUDE_FROM_ALL)

			# Hack to allow clients to #include "scitokens/scitokens.h"
			execute_process(COMMAND ln -s ${scitokens_SOURCE_DIR}/src ${scitokens_SOURCE_DIR}/src/scitokens)
		endif()
		target_include_directories(SciTokens-headers INTERFACE "${scitokens_SOURCE_DIR}/src")
		install(TARGETS SciTokens DESTINATION "${C_LIB}/condor")
		add_dependencies(openssl SciTokens)

	else(NOT PROPER)

		find_path( SCITOKENS_INCLUDE_DIRS NAMES "scitokens/scitokens.h"
			PATHS $ENV{SCITOKENS_DIR}/include ${SCITOKENS_DIR}/include )
		find_library( SCITOKENS_FOUND "SciTokens"
			HINTS $ENV{SCITOKENS_DIR} ${SCITOKENS_DIR}
			PATH_SUFFIXES lib )

		# Fabricate a library target
		add_library(SciTokens SHARED IMPORTED GLOBAL)
		set_target_properties(SciTokens PROPERTIES IMPORTED_LOCATION "${SCITOKENS_FOUND}")

		target_include_directories(SciTokens-headers INTERFACE "/usr/include")

	endif(NOT PROPER OR WANT_SCITOKENS_EXTERNAL)

	# HAVE_EXT_SCITOKENS toggles a #define via config.h.cmake expansion
    # that enables scitokens in C++ code	
	set( HAVE_EXT_SCITOKENS ON PARENT_SCOPE )

	add_dependencies(SciTokens-headers SciTokens)
else(WITH_SCITOKENS)

	message (STATUS "external skipped (SciTokens)")

endif(WITH_SCITOKENS)
