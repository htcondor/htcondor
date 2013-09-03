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
	"${CONDOR_SOURCE_DIR}/src/classad_analysis;${CONDOR_SOURCE_DIR}/src/ccb;${CONDOR_SOURCE_DIR}/src/condor_io;${CONDOR_SOURCE_DIR}/src/condor_ckpt_server;${CONDOR_SOURCE_DIR}/src/condor_daemon_client;${CONDOR_SOURCE_DIR}/src/condor_daemon_core.V6;${CONDOR_SOURCE_DIR}/src/condor_privsep;${CONDOR_SOURCE_DIR}/src/condor_procapi;${CONDOR_SOURCE_DIR}/src/condor_procd;${CONDOR_SOURCE_DIR}/src/condor_schedd.V6;${CONDOR_SOURCE_DIR}/src/condor_sysapi;${CONDOR_SOURCE_DIR}/src/condor_utils;${CONDOR_SOURCE_DIR}/src/condor_ckpt;${CONDOR_SOURCE_DIR}/src/condor_io.std;${CONDOR_SOURCE_DIR}/src/condor_syscall_lib"
	"${CONDOR_SOURCE_DIR}/src/classad;${CONDOR_SOURCE_DIR}/src/safefile")

#if (BUILD_TESTING AND NOT WINDOWS)
if (BUILD_TESTING)
	if (WINDOWS)
		add_custom_target( BLD_TESTS )
		add_dependencies( BLD_TESTS ${CONDOR_TESTS} )
	else(WINDOWS)
		add_custom_target(tests)
		add_dependencies(tests ${CONDOR_TESTS})
	endif(WINDOWS)
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
