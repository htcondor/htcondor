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

		set ( LOCAL_${_CNDR_TARGET} ${_CNDR_TARGET} )

		if ( WINDOWS )
			string (REPLACE ".exe" "" LOCAL_${_CNDR_TARGET} "${LOCAL_${_CNDR_TARGET}}")
			#dprint ("condor_exe_test: ${LOCAL_${_CNDR_TARGET}} : from ${_CNDR_TARGET}")
			APPEND_VAR(CONDOR_TEST_EXES "${LOCAL_${_CNDR_TARGET}}.exe")
		else(WINDOWS)
			APPEND_VAR(CONDOR_TEST_EXES ${_CNDR_TARGET})
		endif( WINDOWS )

		add_executable( ${LOCAL_${_CNDR_TARGET}} EXCLUDE_FROM_ALL ${_SRCS})

		set_target_properties( ${LOCAL_${_CNDR_TARGET}} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${TEST_TARGET_DIR} )
		
		if ( WINDOWS )
			set_property( TARGET ${LOCAL_${_CNDR_TARGET}} PROPERTY FOLDER "tests" )
		endif ( WINDOWS )

		add_dependencies(${LOCAL_${_CNDR_TARGET}} condor_version_obj)
		target_link_libraries(${LOCAL_${_CNDR_TARGET}} PRIVATE "${_LINK_LIBS};condor_version_obj" )

		if ( APPLE )
			add_custom_command( TARGET ${LOCAL_${_CNDR_TARGET}}
				POST_BUILD
				WORKING_DIRECTORY ${TEST_TARGET_DIR}
				COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs ${LOCAL_${_CNDR_TARGET}} )
		endif(APPLE)

		APPEND_VAR( CONDOR_TESTS ${LOCAL_${_CNDR_TARGET}} )

	endif(BUILD_TESTING)

ENDMACRO(CONDOR_EXE_TEST)
