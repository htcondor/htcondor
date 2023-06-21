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
	option(WITH_LOTMAN "Compiling with support for LotMan" ON)
endif(UNIX)

# If we build condor to dlopen the lotman library, we'll need a target that holds
# the header file include directory, but no library code.
add_library(LotMan-headers INTERFACE)

if (WITH_LOTMAN)

	if ( NOT PROPER OR WANT_LOTMAN_EXTERNAL )
		include(FetchContent)

		# This runs the wget at cmake time, runs the cmake in the tarball, and sets a bunch of scitokens_ variables
		FetchContent_declare(
			scitokens
			URL ${EXTERNALS_SOURCE_URL}/lotman-0.0.1.tar.gz
			#URL_HASH MD5=23af2833a3f4459344913d18385a6473
		)

		# Check if population has already been performed
		FetchContent_GetProperties(lotman)
		if(NOT lotman_POPULATED)
			# Fetch the content using previously declared details
			FetchContent_Populate(lotman)

			# Load the lotman cmake file, but exclude the install target, so nothing gets installed
			add_subdirectory(${lotman_SOURCE_DIR} ${lotman_BINARY_DIR} EXCLUDE_FROM_ALL)

			# Hack to allow clients to #include "lotman/lotman.h"
			exec_program("ln -s ${lotman_SOURCE_DIR}/src ${lotman_SOURCE_DIR}/src/lotman")
		endif()
		target_include_directories(LotMan-headers INTERFACE "${lotman_SOURCE_DIR}/src")
		install(TARGETS LotMan DESTINATION "${C_LIB}/condor")
		add_dependencies(SciTokens)

	else(NOT PROPER)

		find_path( LOTMAN_INCLUDE_DIRS NAMES "lotman/lotman.h"
			PATHS $ENV{LOTMAN_DIR}/include ${LOTMAN_DIR}/include )
		find_library( LOTMAN_FOUND "LotMan"
			HINTS $ENV{LOTMAN_DIR} ${LOTMAN_DIR}
			PATH_SUFFIXES lib )

		# Fabricate a library target
		add_library(LotMan SHARED IMPORTED GLOBAL)
		set_target_properties(LotMan PROPERTIES IMPORTED_LOCATION "${LOTMAN_FOUND}")

		target_include_directories(LotMan-headers INTERFACE "/usr/include")

	endif(NOT PROPER OR WANT_LOTMAN_EXTERNAL)

	# HAVE_EXT_LOTMAN toggles a #define via config.h.cmake expansion
    # that enables lotman in C++ code	
	set( HAVE_EXT_LOTMAN ON PARENT_SCOPE )

	add_dependencies(LotMan-headers LotMan)
else(WITH_LOTMAN)

	message (STATUS "external skipped (LotMan)")

endif(WITH_LOTMAN)
