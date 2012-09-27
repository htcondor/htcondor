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

	file( GLOB negotiatorRmvElements Example* accountant_log_fixer.cpp )
	if (UNIX)
	set_source_files_properties(matchmaker.cpp main.cpp Accountant.cpp PROPERTIES COMPILE_FLAGS -Wno-float-equal)
	endif(UNIX)

	condor_daemon( negotiator "${negotiatorRmvElements}"
	"${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${CONDOR_QMF}" "${C_SBIN}" OFF )

endif(NOT WIN_EXEC_NODE_ONLY)

condor_exe(accountant_log_fixer "accountant_log_fixer.cpp" ${C_LIBEXEC} "" OFF)
