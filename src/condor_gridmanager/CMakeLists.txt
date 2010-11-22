 ###############################################################
 # 
 # Copyright (C) 1990-2010, Redhat. 
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


if(NOT WIN_EXEC_NODE_ONLY)
	set (GM_LINK_LIBS "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${COREDUMPER_FOUND}")

	file( GLOB RmvSrcs *_gahp_wrapper* )
	condor_glob( GMHDRS GMSRCS "${RmvSrcs}" )

	condor_exe( condor_gridmanager "${GMHDRS};${GMSRCS}" ${C_SBIN} "qmgmt;daemon_core;${GM_LINK_LIBS}" )

	if (NOT WINDOWS)
		condor_exe( gt4_gahp "gt4_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" )
		condor_exe( gt42_gahp "gt42_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" )
		condor_exe( unicore_gahp "unicore_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" )
	endif()

	install (FILES gridftp_wrapper.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

endif(NOT WIN_EXEC_NODE_ONLY)
