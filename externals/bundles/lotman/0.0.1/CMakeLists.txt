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

# Set default not to use LotMan
option(WITH_LOTMAN "Compiling with support for LotMan" OFF)

if ((LINUX_NAME MATCHES "AlmaLinux" OR LINUX_NAME MATCHES "RedHat") AND (LINUX_VER MATCHES "^[89][.]") AND (SYS_ARCH MATCHES "X86_64"))
	set(WITH_LOTMAN ON CACHE BOOL "Lotman support turned on based on OS/ARCH" FORCE)
	message("LotMan support turned on based on detected OS/ARCH")
endif()

# If we build condor to dlopen the lotman library, we'll need a target that holds
# the header file include directory, but no library code.
add_library(LotMan-headers INTERFACE)

if (WITH_LOTMAN)

	if ( NOT PROPER OR WANT_LOTMAN_EXTERNAL )
		include(FetchContent)

		FetchContent_declare(
			lotman
			GIT_REPOSITORY https://github.com/PelicanPlatform/lotman.git
			GIT_TAG        f420248b1401fce3ece1ad86c37287321834c61e # release-0.0.2
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
		add_dependencies(LotMan)

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
