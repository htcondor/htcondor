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

include(CMakeParseArguments)

MACRO ( CONDOR_PL_TEST _TARGET _DESC _TEST_RUNS )

	if (BUILD_TESTING)

		cmake_parse_arguments(${_TARGET} "CTEST;JAVA" "" "DEPENDS" ${ARGN})

		if ( ${_TARGET}_CTEST )
			if ( ${_TARGET}_JAVA )
				ADD_TEST(${_TARGET} ${PYTHON3_EXECUTABLE}
					${CMAKE_SOURCE_DIR}/src/condor_tests/ctest_driver.py
					"--test" ${_TARGET}
					"--working-dir" ${CMAKE_BINARY_DIR}
					"--source-dir" ${CMAKE_SOURCE_DIR}
					"--prefix-path" ${CMAKE_INSTALL_PREFIX}
					"--dependencies" "${${_TARGET}_DEPENDS}"
					"--java")
			else()
				ADD_TEST(${_TARGET} ${PYTHON3_EXECUTABLE}
					${CMAKE_SOURCE_DIR}/src/condor_tests/ctest_driver.py
					"--test" ${_TARGET}
					"--working-dir" ${CMAKE_BINARY_DIR}
					"--source-dir" ${CMAKE_SOURCE_DIR}
					"--prefix-path" ${CMAKE_INSTALL_PREFIX}
					"--dependencies" "${${_TARGET}_DEPENDS}")

			endif()
			set_tests_properties(${_TARGET} PROPERTIES LABELS "${_TEST_RUNS}")
			add_custom_target(${_TARGET} ALL)
		endif()

		foreach(test ${_TEST_RUNS})
			file (APPEND ${TEST_TARGET_DIR}/list_${test} "${_TARGET}\n")
			APPEND_UNIQUE_VAR(CONDOR_TEST_LIST_TAGS ${test})
		endforeach(test)
		
		# add to all targets 
		file (APPEND ${TEST_TARGET_DIR}/list_all "${_TARGET}\n")

		# I'm not certain but it appears that the description files are not gen'd
		# file ( APPEND ${TEST_TARGET_DIR}/${_TARGET}.desc ${_DESC} )

		APPEND_VAR( CONDOR_PL_TESTS ${_TARGET} )

	endif (BUILD_TESTING)

ENDMACRO ( CONDOR_PL_TEST )
