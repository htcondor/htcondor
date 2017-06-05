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

file(GLOB FTSrcs "FTEST_*.cpp")
file(GLOB OTSrcs "OTEST_*.cpp")

# old style condor unit tests that all link into a single huge exe
condor_exe_test(condor_unit_tests "${FTSrcs};${OTSrcs};emit.cpp;function_test_driver.cpp;unit_test_utils.cpp;unit_tests.cpp" "${CONDOR_TOOL_LIBS};${CONDOR_WIN_LIBS}")

# formly boost-testy unit tests that each link to a stand-alone exe
condor_unit_test ( _ring_buffer_tester ring_buffer_tests.cpp "" OFF )
condor_unit_test ( _consumption_policy_tester consumption_policy_tests.cpp "condor_utils" OFF )


