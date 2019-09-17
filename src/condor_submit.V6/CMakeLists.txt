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

include_directories(${CONDOR_SCHEDD_SRC_DIR})

condor_exe( condor_submit "submit.cpp;submit_simulate.cpp;submit_factory.cpp;submit_help.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF )

# simple program to test"late" materialization outside of the schedd 
condor_exe_test( test_job_factory.exe "factory_test.cpp" "${CONDOR_TOOL_LIBS}" )
