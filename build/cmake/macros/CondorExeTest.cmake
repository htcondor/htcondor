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

MACRO (CONDOR_EXE_TEST _CNDR_TARGET _SRCS _LINK_LIBS )

	if (BUILD_TESTING)

		# The logical CMake target name is always the base name, with any trailing
		# .exe removed, so it is identical on every platform.  That lets callers use
		# the plain name in add_dependencies() and $<TARGET_FILE:...> generator
		# expressions without worrying about the on-disk suffix.
		string (REGEX REPLACE "\\.exe$" "" _cet_target "${_CNDR_TARGET}")

		# A test whose submit/.cmd files name the binary <target>.exe needs that
		# exact file name on *every* platform (not just Windows).  Such a test
		# passes the EXE keyword; we then force the .exe suffix so the on-disk file
		# is <target>.exe even on Unix.  (Naming the target with a trailing .exe is
		# also honored for backward compatibility.)  Without it, the binary uses the
		# platform default: <target>.exe on Windows, plain <target> on Unix.
		set(_cet_force_exe FALSE)
		if ( NOT "${_cet_target}" STREQUAL "${_CNDR_TARGET}" )
			set(_cet_force_exe TRUE)
		endif()
		# Note: inside a macro ARGN is a string substitution, not a real list
		# variable, so it must be expanded with ${ARGN} rather than IN LISTS ARGN.
		foreach(_cet_arg ${ARGN})
			if ("${_cet_arg}" STREQUAL "EXE")
				set(_cet_force_exe TRUE)
			endif()
		endforeach()

		add_executable( ${_cet_target} EXCLUDE_FROM_ALL ${_SRCS})
		set_target_properties( ${_cet_target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR} )

		if ( _cet_force_exe )
			set_target_properties( ${_cet_target} PROPERTIES SUFFIX ".exe" )
			APPEND_VAR(CONDOR_TEST_EXES "${_cet_target}.exe")
		elseif ( WINDOWS )
			# Plain name: on Windows CMake appends its default .exe suffix.
			APPEND_VAR(CONDOR_TEST_EXES "${_cet_target}.exe")
		else()
			APPEND_VAR(CONDOR_TEST_EXES "${_cet_target}")
		endif()

		if ( WINDOWS )
			set_property( TARGET ${_cet_target} PROPERTY FOLDER "tests" )
		endif ( WINDOWS )

		add_dependencies(${_cet_target} condor_version_obj)
		target_link_libraries(${_cet_target} PRIVATE "${_LINK_LIBS};condor_version_obj" )

		if ( APPLE )
			# Pass the actual built file, not the target name: with the .exe suffix
			# forced on some targets the two differ (target x_foo -> file x_foo.exe),
			# and macosx_rewrite_libs must be handed a real filename or it silently
			# skips it and leaves the dylib load paths unrewritten.
			add_custom_command( TARGET ${_cet_target}
				POST_BUILD
				WORKING_DIRECTORY ${TEST_TARGET_DIR}
				COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs $<TARGET_FILE:${_cet_target}> )
		endif(APPLE)

		APPEND_VAR( CONDOR_TESTS ${_cet_target} )

	endif(BUILD_TESTING)

ENDMACRO(CONDOR_EXE_TEST)
