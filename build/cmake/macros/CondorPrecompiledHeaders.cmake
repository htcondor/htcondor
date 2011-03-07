 ###############################################################
 #
 # Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

# This macro will setup to use precompiled headers on compilers that support it.
# NOTE: the macro expects to have a list of sources passed via global variable
# named _SRCS, and it may edit the files in _SRCS appropriately.
# This resulting _SRCS variable value should then be passed to add_library() 
# or add_executable().
MACRO(ADD_PRECOMPILED_HEADER)

  list(LENGTH _SRCS _SRCS_LENGTH)
  # For now only precompile headers on Microsoft C++, and for targets
  # that have more than 90 source files (essentially utils and condorapi).
  # Once we figure out how to safely use the same precompiled 
  # header across targets, we could lower the source
  # file count from 90 to something much less, like 5.
  if (MSVC AND (${_SRCS_LENGTH} GREATER 90))

	dprint("Will use precompiled headers for this target")
  
	SET(PrecompiledSource "${PROJECT_SOURCE_DIR}/src/condor_utils/condor_common.cpp")
	SET(PrecompiledBinary "\$(TargetDir)\\condor_common.\$(TargetName).pch")
	SET(FoundCondorCommon FALSE)
	SET(UseCondorCommon FALSE)
	foreach( src_file ${_SRCS} )
		if (${src_file} STREQUAL ${PrecompiledSource})
				SET(FoundCondorCommon TRUE)
		elseif( (${src_file} MATCHES ".cpp$" AND EXISTS ${src_file}) )
				file(READ ${src_file} f0 LIMIT 4096)
				# Scan via a regex to find the first pre-processor macro
				string(REGEX MATCH "(^|[\r\n])[ \t]*#[ \t]*([^ \t\r\n]+)" f1 ${f0})
				set(FirstPoundDef "${CMAKE_MATCH_2}")
				# Now scan via a regex to find the first file #included
				string(REGEX MATCH "(^|[\r\n])[ \t]*#include[ \t]+[\"<]([^\">]+)" f1 ${f0})
				# Use precompiled headers on this file IF the first pre-processor
				# macro happens to be a #include AND the first include is condor_common.h.
				# This is important because, for instance, we should not use precompiled 
				# headers on a file that does #defines before the #include "condor_common.h"
				if  ("${CMAKE_MATCH_2}" STREQUAL "condor_common.h" AND
					 "${FirstPoundDef}" STREQUAL "include" )
					SET(UseCondorCommon TRUE)
					set_source_files_properties(
						${src_file}
						PROPERTIES
						COMPILE_FLAGS "/Yu\"condor_common.h\" /Fp\"${PrecompiledBinary}\""
						OBJECT_DEPENDS "${PrecompiledBinary}"
						)
				else()
					dprint("file ${src_file} NOT using condor_common.h first")
				endif()
		endif() 
    endforeach()

	if(UseCondorCommon)
		if(NOT FoundCondorCommon)
			LIST(APPEND _SRCS "${PrecompiledSource}")
		endif()
    	set_source_files_properties(
			${PrecompiledSource}
			PROPERTIES 
			COMPILE_FLAGS "/Yc\"condor_common.h\" /Fp\"${PrecompiledBinary}\""
			OBJECT_OUTPUTS "${PrecompiledBinary}"
			)
	endif(UseCondorCommon)


  endif()  # of if MSVC and project has enough targets to be worth our time

ENDMACRO(ADD_PRECOMPILED_HEADER)

