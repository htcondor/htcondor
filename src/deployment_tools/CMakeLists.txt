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

if (NOT WINDOWS AND WANT_FULL_DEPLOYMENT)

	install ( FILES cleanup_release
				condor_cleanup_local
				condor_cold_start
				condor_cold_stop
				condor_config_bind
				filelock_midwife
				filelock_undertaker
				condor_install_local
				condor_local_start
				condor_local_stop
				install_release
			DESTINATION ${C_SBIN}
			PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

	install ( FILES Execute.pm FileLock.pm DESTINATION ${C_LIB} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

	condor_exe( uniq_pid_command "uniq_pid_tool_main.cpp" "${C_SBIN}" "${CONDOR_TOOL_LIBS};${KRB5_FOUND};${OPENSSL_FOUND};${PCRE_FOUND}" OFF )
	clone_install( uniq_pid_command "${C_SBIN}" uniq_pid_midwife "${C_SBIN}" )
	clone_install( uniq_pid_command "${C_SBIN}" uniq_pid_undertaker "${C_SBIN}" )

endif (NOT WINDOWS AND WANT_FULL_DEPLOYMENT)
