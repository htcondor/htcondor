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

condor_exe( condor_rm "rm.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}" OFF )
# unfortunately libutils comes with undefined symbols introduced by
# libdaemon_core (even when linking libutils to it)
target_link_libraries(condor_rm -Wl,--allow-shlib-undefined)
clone_install( condor_rm "${C_BIN}" condor_hold "${C_BIN}" )
clone_install( condor_rm "${C_BIN}" condor_release "${C_BIN}" )
clone_install( condor_rm "${C_BIN}" condor_vacate_job "${C_BIN}" )
