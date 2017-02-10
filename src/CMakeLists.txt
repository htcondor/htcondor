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
	"${CONDOR_SOURCE_DIR}/src/bash_completion.d;${CONDOR_SOURCE_DIR}/src/classad_analysis;${CONDOR_SOURCE_DIR}/src/ccb;${CONDOR_SOURCE_DIR}/src/condor_io;${CONDOR_SOURCE_DIR}/src/condor_ckpt_server;${CONDOR_SOURCE_DIR}/src/condor_daemon_client;${CONDOR_SOURCE_DIR}/src/condor_daemon_core.V6;${CONDOR_SOURCE_DIR}/src/condor_privsep;${CONDOR_SOURCE_DIR}/src/condor_procapi;${CONDOR_SOURCE_DIR}/src/condor_procd;${CONDOR_SOURCE_DIR}/src/condor_schedd.V6;${CONDOR_SOURCE_DIR}/src/condor_sysapi;${CONDOR_SOURCE_DIR}/src/condor_utils;${CONDOR_SOURCE_DIR}/src/condor_ckpt;${CONDOR_SOURCE_DIR}/src/condor_io.std;${CONDOR_SOURCE_DIR}/src/condor_syscall_lib"
	"${CONDOR_SOURCE_DIR}/src/classad;${CONDOR_SOURCE_DIR}/src/safefile")

#if (BUILD_TESTING AND NOT WINDOWS)
if (BUILD_TESTING)
	if (WINDOWS)
		add_custom_target( BLD_TESTS )
		add_dependencies( BLD_TESTS ${CONDOR_TESTS} )
		set(COPY_COMMAND xcopy /y)
		set(COPY_RECURSIVE_COMMAND xcopy /ys)
	else(WINDOWS)
		add_custom_target(tests)
		add_dependencies(tests ${CONDOR_TESTS})
		set(COPY_COMMAND cp)
		set(COPY_RECURSIVE_COMMAND cp -r)
	endif(WINDOWS)

	# create the directory for copying the tests, and copy all the perl and binary tests
	# currently we do not copy any payload required by the tests. Will need to figure out
	# if the payload file names follow a particular format, so that it will be easy to
	# copy all of them using regex/wildcards.
	add_custom_target(tests-tar-pkg
	    DEPENDS tests
	    COMMAND ${CMAKE_COMMAND} -E make_directory ${CPACK_TEST_PACKAGE_FILE_NAME}
	    COMMAND ${COPY_RECURSIVE_COMMAND} ${CMAKE_CURRENT_BINARY_DIR}/condor_tests/*.exe* ${CPACK_TEST_PACKAGE_FILE_NAME}
	    COMMAND ${COPY_RECURSIVE_COMMAND} ${CONDOR_SOURCE_DIR}/src/condor_tests/*.pl ${CONDOR_SOURCE_DIR}/src/condor_tests/*.pm ${CONDOR_SOURCE_DIR}/src/condor_tests/*.run ${CPACK_TEST_PACKAGE_FILE_NAME}
	)

	foreach(scriptDir Check
               ProcInfos
               StatusAds
               StatusConfigs
               Includes
               gsi)
            add_custom_command(
                TARGET tests-tar-pkg
                POST_BUILD
                COMMAND ${COPY_RECURSIVE_COMMAND} ${CONDOR_SOURCE_DIR}/src/condor_tests/${scriptDir} ${CPACK_TEST_PACKAGE_FILE_NAME}
            )
	endforeach(scriptDir)

	set(CONDOR_SCRIPTS_DIR ${CONDOR_SOURCE_DIR}/src/condor_scripts)
	foreach(scriptFile Condor.pm
			   CondorTest.pm
			   CondorPersonal.pm
			   CondorUtils.pm
			   CheckOutputFormats.pm
			   run_test.pl)
            add_custom_command(
                TARGET tests-tar-pkg
                POST_BUILD
                COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CONDOR_SCRIPTS_DIR}/${scriptFile} ${CPACK_TEST_PACKAGE_FILE_NAME}
            )
	endforeach(scriptFile)

	add_custom_command(
	    TARGET tests-tar-pkg
	    POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E tar czf ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz ${CPACK_TEST_PACKAGE_FILE_NAME}
	    COMMAND ${CMAKE_COMMAND} -E copy ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz ${CMAKE_BINARY_DIR}
	    COMMAND ${CMAKE_COMMAND} -E remove -f ${CPACK_TEST_PACKAGE_FILE_NAME}.tar.gz
	    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CPACK_TEST_PACKAGE_FILE_NAME}
	)

	dprint("CONDOR_TESTS=${CONDOR_TESTS}")
endif()

if (WANT_FULL_DEPLOYMENT)
	install ( FILES condor_includes/condor_classad.h
			  condor_includes/condor_constants.h
			  condor_includes/condor_header_features.h
			  condor_includes/file_lock.h
			  condor_utils/iso_dates.h
			DESTINATION ${C_INCLUDE})
endif(WANT_FULL_DEPLOYMENT)
