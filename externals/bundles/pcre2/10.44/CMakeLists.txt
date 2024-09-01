###############################################################
#
# Copyright (C) 2024, Condor Team, Computer Sciences Department,
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

# If proper, we'll find pcre2 from an installed package on the local filesytem
# Otherwise, download and install it locally

if ( NOT PROPER )
	# Some PCRE2 settings 
	set(PCRE2_BUILD_TESTS OFF CACHE INTERNAL "") # No need to build the pcre test suite
	set(PCRE2_BUILD_PCRE2GREP OFF CACHE INTERNAL "")  # No need to build the pcre2grep binary
	set(BUILD_SHARED_LIBS ON CACHE INTERNAL "")  # Build shared pcre lib
	set(BUILD_STATIC_LIBS OFF CACHE INTERNAL "")  # But not a static one

	include(FetchContent)

	# This runs the wget at cmake time, runs the cmake in the tarball, and sets a bunch of pcre2_ variables
	FetchContent_declare(
			pcre2
			URL ${EXTERNALS_SOURCE_URL}/pcre2-10.44.tar.gz
			URL_HASH MD5=55680e6d658bf119976cac83a448c829
			)

	# Check if population has already been performed
	FetchContent_GetProperties(pcre2)
	if(NOT pcre2_POPULATED)
		# Fetch the content using previously declared details
		FetchContent_Populate(pcre2)

		# Load the pcre2 cmake file, but exclude the install target, so nothing gets installed
		add_subdirectory(${pcre2_SOURCE_DIR} ${pcre2_BINARY_DIR} EXCLUDE_FROM_ALL)
	endif()

	# pcre2-8 is the cmake target for 8 bit characters, which expands to the proper platform-specific filename(s)
	set(PCRE2_FOUND pcre2-8)

	# pcre2 10.41 will do this for us, but for now attach the include dir to the library target
	target_include_directories(pcre2-8-shared INTERFACE "${pcre2_BINARY_DIR}")

	# Remove this when we drop Ubuntu 18 (and cmake 3.10)
	set(PCRE2_INCLUDE ${pcre2_BINARY_DIR})
	set(PCRE2_INCLUDE ${PCRE2_INCLUDE} PARENT_SCOPE)

	# PCRE2_REF is used as the dependency to chain classads/condor_utils to pcre2
	set(PCRE2_REF "${PCRE2_FOUND}")
	set(PCRE2_REF ${PCRE2_REF} PARENT_SCOPE)

	# We turned off pcre2 install rule, just install the shared libraries in our directory
	if (UNIX)
		set_target_properties(pcre2-8-shared PROPERTIES INSTALL_NAME_DIR "${C_LIB}/condor")
	endif()
	
	if (APPLE)
		set_target_properties(pcre2-8-shared PROPERTIES INSTALL_RPATH "@executable_path/../lib/condor")
	endif()

	if (UNIX)
		install(TARGETS pcre2-8-shared 
			RUNTIME DESTINATION "${C_LIB}/condor"
			LIBRARY DESTINATION "${C_LIB}/condor")
	else()
		install(TARGETS pcre2-8-shared 
			RUNTIME DESTINATION "${C_LIB}"
			LIBRARY DESTINATION "${C_LIB}")
	endif()
else()
	# Proper, i.e. pre-installed 
	find_library(PCRE2_FOUND "pcre2-8")
	
	# Make a library target for the installed on disk shared library
	# that looks just like a target we would build
	add_library(pcre2-8 SHARED IMPORTED GLOBAL)
	set_target_properties(pcre2-8 PROPERTIES IMPORTED_LOCATION "${PCRE2_FOUND}")

endif( NOT PROPER)

## Hard failure if there is no PCRE2
if (PCRE2_FOUND)
	message (STATUS "external configured (PCRE2_FOUND=${PCRE2_FOUND})")

	# Globalize the variable
	set(HAVE_EXT_PCRE2 ON PARENT_SCOPE )
else()
	message (FATAL_ERROR "pcre2 not found and is required to build")
endif(PCRE2_FOUND)
