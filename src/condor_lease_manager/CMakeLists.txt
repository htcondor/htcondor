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


if (NOT WINDOWS)
	if (WANT_LEASE_MANAGER)

		file( GLOB rmvLMFiles *test*)

		condor_glob(LMHdrs LMSrcs "${rmvLMFiles}*")

		condor_exe( condor_lease_manager "${LMHdrs};${LMSrcs}" ${C_SBIN}
		"${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}" OFF )

		condor_exe_test(condor_lease_manager_test "lease_manager_test.cpp" "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}")
		condor_exe_test(condor_lease_manager_tester "lease_manager_tester.cpp" "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}")

	else(WANT_LEASE_MANAGER)
		message(STATUS "NOTE: *lease_manager* targets will not be built WANT_LEASE_MANAGER=${WANT_LEASE_MANAGER}")
	endif(WANT_LEASE_MANAGER)
endif()
