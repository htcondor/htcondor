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

	set (JR_LINK_LIBS "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}")

	condor_glob(JRHdrs JRSrcs "VanillaToGrid_main.cpp")

	condor_exe( condor_job_router "${JRHdrs};${JRSrcs}" ${C_LIBEXEC} "${JR_LINK_LIBS}" OFF )

	if (WINDOWS)

			# windows install requires scripts have the correct extension in order 
			# to execute correctly by default.
			install ( FILES condor_router_history
					DESTINATION ${C_BIN} 
					RENAME condor_router_history.py)

			install ( FILES condor_router_q
					DESTINATION ${C_BIN} 
					RENAME condor_router_q.sh)

			install ( FILES condor_router_rm
					DESTINATION ${C_BIN} 
					RENAME condor_router_rm.sh)

	else()

		install ( FILES condor_router_history condor_router_q condor_router_rm 
				DESTINATION ${C_BIN} 
				PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

	endif()

endif()
