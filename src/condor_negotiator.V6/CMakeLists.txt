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

set(negotiatorElements
Accountant.cpp
GroupEntry.cpp
main.cpp
matchmaker.cpp
matchmaker_negotiate.cpp
NegotiatorPluginManager.cpp
)

if (UNIX)
		set_source_files_properties(matchmaker.cpp main.cpp Accountant.cpp GroupEntry.cpp hgq_group_tester.cpp PROPERTIES COMPILE_FLAGS -Wno-float-equal)
endif(UNIX)

condor_daemon( EXE condor_negotiator SOURCES "${negotiatorElements}"
  LIBRARIES "${CONDOR_LIBS};${CONDOR_QMF}" INSTALL "${C_SBIN}" )

condor_exe_test( test_protocol_matching
  "protocol-test.cpp;matchmaker.cpp;Accountant.cpp;GroupEntry.cpp;matchmaker_negotiate.cpp"
  "${CONDOR_LIBS}" )

condor_exe(accountant_log_fixer "accountant_log_fixer.cpp" ${C_LIBEXEC} "" OFF)
#condor_exe(hgq_group_tester "hgq_group_tester.cpp;GroupEntry.cpp" ${C_BIN} "${CONDOR_LIBS}" OFF)
