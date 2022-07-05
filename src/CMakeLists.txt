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

##########################################################################
# adds all subtree elements which have a CMakeLists.txt file exclude
# classads b/c we added earlier to elim externals dependency - all externals reference
# one version of classads which is ${CONDOR_SOURCE_DIR}/src/classad

# the 2nd arg is prio order for symbols ripping to prevent .o copy-mindging within the std:u
add_subdirectories("${CONDOR_SOURCE_DIR}/src"
	"${CONDOR_SOURCE_DIR}/src/bash_completion.d;${CONDOR_SOURCE_DIR}/src/classad_analysis;${CONDOR_SOURCE_DIR}/src/ccb;${CONDOR_SOURCE_DIR}/src/condor_io;${CONDOR_SOURCE_DIR}/src/condor_daemon_client;${CONDOR_SOURCE_DIR}/src/condor_daemon_core.V6;${CONDOR_SOURCE_DIR}/src/condor_procapi;${CONDOR_SOURCE_DIR}/src/condor_procd;${CONDOR_SOURCE_DIR}/src/condor_schedd.V6;${CONDOR_SOURCE_DIR}/src/condor_sysapi;${CONDOR_SOURCE_DIR}/src/condor_utils"
	"${CONDOR_SOURCE_DIR}/src/classad;${CONDOR_SOURCE_DIR}/src/safefile")

#if (BUILD_TESTING AND NOT WINDOWS)
if (BUILD_TESTING)
	if (WINDOWS)
		add_custom_target( BLD_TESTS )
		add_dependencies( BLD_TESTS ${CONDOR_TESTS} )
		#set(COPY_COMMAND xcopy /y)
		#set(COPY_RECURSIVE_COMMAND xcopy /ys)
	else(WINDOWS)
		add_custom_target(tests)
		add_dependencies(tests ${CONDOR_TESTS})
		#set(COPY_COMMAND cp)
		#set(COPY_RECURSIVE_COMMAND cp -r)
	endif(WINDOWS)

	# This is a bit ugly, but to make a package of tests, what we have to do is build a cmake script file
	# using FILE(WRITE/APPEND) commands that has inside it FILE(INSTALL) commands to build up the 
	# directory tree of tests and test dependencies that we want to package.
	#
	# we then define a custom target that invokes this cmake script file and then tars up the result
	#
	set(COPY_TESTS_CMAKE_FILE "${CMAKE_CURRENT_BINARY_DIR}/copy_tests.cmake")
	FILE(WRITE ${COPY_TESTS_CMAKE_FILE}
		"# generated cmake file to copy tests\n"
		"IF(NOT DEFINED CMAKE_INSTALL_PREFIX)\n"
		"  SET(CMAKE_INSTALL_PREFIX \"${CMAKE_BINARY_DIR}/release_dir\")\n"
	"ENDIF()\n"
	)
	if (WINDOWS)
		FILE(APPEND ${COPY_TESTS_CMAKE_FILE}
			"IF(NOT DEFINED BUILD_TYPE)\n"
			"  SET(BUILD_TYPE RelWithDebInfo)\n"
			"ENDIF()\n"
		)
	endif(WINDOWS)

	# recursive copy the condor_tests source directory
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "FILE(INSTALL \"${CMAKE_CURRENT_SOURCE_DIR}/condor_tests\" DESTINATION \${CMAKE_INSTALL_PREFIX})\n")

	# setup a big copy list for the tests themselves
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "FILE(INSTALL " "DESTINATION \${CMAKE_INSTALL_PREFIX}/condor_tests TYPE EXECUTABLE FILES\n")
	# 
	FOREACH (test IN LISTS CONDOR_TEST_EXES)
		if (WINDOWS)
			FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_BINARY_DIR}/condor_tests/\${BUILD_TYPE}/${test}\"\n")
		else (WINDOWS)
			FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_BINARY_DIR}/condor_tests/${test}\" \n")
		endif(WINDOWS)
	ENDFOREACH (test)
	# copy rules for pl tests
	FOREACH (test IN LISTS CONDOR_PL_TESTS)
		IF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/condor_tests/${test}.run)
			FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_SOURCE_DIR}/condor_tests/${test}.run\" \n")
		ELSEIF(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/condor_tests/${test}.py)
			FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_SOURCE_DIR}/condor_tests/${test}.py\" \n")
		ELSE()
			# hack to trigger the same "cannot find file" as would appear with no .py check
			FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_SOURCE_DIR}/condor_tests/${test}.run\" \n")
		ENDIF()
	ENDFOREACH (test)
	# copy rules for tests helper scripts
	FOREACH (script IN LISTS CONDOR_TEST_HELPER_SCRIPTS)
		FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_SOURCE_DIR}/condor_scripts/${script}\" \n")
	ENDFOREACH (script)
	# close the big copy list
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_SOURCE_DIR}/msconfig/do_tests.pl\" \n")
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} ")\n")

	# copy the generated test lists
	APPEND_UNIQUE_VAR(CONDOR_TEST_LIST_TAGS "all")
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "FILE(INSTALL " "DESTINATION \${CMAKE_INSTALL_PREFIX}/condor_tests TYPE FILE FILES\n")
	#FOREACH (tag "quick" "all" "ckpt" "core" "condorc" "critical" "dagman" "filexfer" "framework" "full" "java" "lib" "local" "long" "rsc" "simple" "stduniv" "time")
	FOREACH (tag IN LISTS CONDOR_TEST_LIST_TAGS)
		FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${TEST_TARGET_DIR}/list_${tag}\"\n")
	ENDFOREACH(tag)
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_BINARY_DIR}/condor_utils/command_name_tables.h\"\n")
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} "  \"${CMAKE_CURRENT_BINARY_DIR}/condor_utils/param_info_tables.h\"\n")
	FILE(APPEND ${COPY_TESTS_CMAKE_FILE} ")\n")

	IF(WINDOWS)
		FILE(TO_NATIVE_PATH "${COPY_TESTS_CMAKE_FILE}" WIN_COPY_TESTS_FILE)
		add_custom_target(tests-tar-pkg
			DEPENDS BLD_TESTS
			COMMAND ${CMAKE_COMMAND} -DBUILD_TYPE=$(Configuration) -DCMAKE_INSTALL_PREFIX=${CPACK_TEST_PACKAGE_FILE_NAME} -P "${WIN_COPY_TESTS_FILE}"
		)
	ELSE(WINDOWS)
		add_custom_target(tests-tar-pkg
			DEPENDS tests
			COMMAND ${CMAKE_COMMAND} -DCMAKE_INSTALL_PREFIX=${CPACK_TEST_PACKAGE_FILE_NAME} -P ${COPY_TESTS_CMAKE_FILE}
		)
	ENDIF(WINDOWS)

	add_custom_command(
	    TARGET tests-tar-pkg
	    POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E tar czf ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz ${CPACK_TEST_PACKAGE_FILE_NAME}
	    COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz ${CMAKE_BINARY_DIR}
	    COMMAND ${CMAKE_COMMAND} -E remove -f ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz
	    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CPACK_TEST_PACKAGE_FILE_NAME}
	)

	dprint("CONDOR_TEST_LIST_TAGS=${CONDOR_TEST_LIST_TAGS}")
	dprint("CONDOR_TESTS=${CONDOR_TESTS}")
endif()

install ( FILES condor_includes/file_lock.h
		DESTINATION ${C_INCLUDE})
