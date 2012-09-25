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

if (NOT WIN_EXEC_NODE_ONLY)

	condor_selective_glob( "Pipe*;io*;ftgahp*" FTGahpSrcs )

	condor_exe( condor_ft-gahp "${FTGahpSrcs}" ${C_SBIN} "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${COREDUMPER_FOUND}" OFF )

endif(NOT WIN_EXEC_NODE_ONLY)
