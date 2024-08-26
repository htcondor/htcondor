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


set(CMAKE_UNITY_BUILD False)

set( FTSrcs
FTEST_basename.cpp
FTEST_dirname.cpp
FTEST_flatten_and_inline.cpp
FTEST_fullpath.cpp
FTEST_getHostFromAddr.cpp
FTEST_getPortFromAddr.cpp
FTEST_host_in_domain.cpp
FTEST_is_valid_sinful.cpp
FTEST_stl_string_utils.cpp
FTEST_string_vector.cpp
FTEST_string_to_port.cpp
FTEST_strlwr.cpp
FTEST_strupr.cpp
FTEST_tokener.cpp
FTEST_your_string.cpp
)
set( OTSrcs
OTEST_ArgList.cpp
OTEST_condor_sockaddr.cpp
OTEST_Directory.cpp
OTEST_Env.cpp
OTEST_FileLock.cpp
OTEST_HashTable.cpp
OTEST_Regex.cpp
OTEST_Iso_Dates.cpp
OTEST_Old_Classads.cpp
OTEST_ranger.cpp
OTEST_StatInfo.cpp
OTEST_Timeslice.cpp
OTEST_TmpDir.cpp
OTEST_UserPolicy.cpp
)

# old style condor unit tests that all link into a single huge exe
condor_exe_test(condor_unit_tests "${FTSrcs};${OTSrcs};emit.cpp;function_test_driver.cpp;unit_test_utils.cpp;unit_tests.cpp" "${CONDOR_TOOL_LIBS};${CONDOR_WIN_LIBS}")

# stand-alone test for async reader class since it needs to generate test files
condor_exe_test(async_freader_tests async_freader_tests.cpp "${CONDOR_TOOL_LIBS};${CONDOR_WIN_LIBS}")

# formly boost-testy unit tests that each link to a stand-alone exe
condor_exe_test ( _ring_buffer_tester ring_buffer_tests.cpp "" OFF )
condor_exe_test ( _consumption_policy_tester consumption_policy_tests.cpp "condor_utils" OFF )


