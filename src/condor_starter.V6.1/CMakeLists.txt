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


file( GLOB starterRmvElements condor_rsh.cpp glexec_job_wrapper* )

set_source_files_properties(jic_shadow.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")

condor_daemon( starter "${starterRmvElements}" "${CONDOR_LIBS};${LIBCGROUP_FOUND}" "${C_SBIN}" OFF)

if (HAVE_EXT_GLOBUS)
	clone_install( condor_starter "${C_SBIN}" "condor_gridshell" "${C_SBIN}" )
endif()

# need to add all the other install targets for libexec.
install ( FILES scimark2lib.jar CondorJavaWrapper.class CondorJavaInfo.class DESTINATION ${C_LIB} )

if(LINUX AND WANT_GLEXEC)
	install (FILES condor_glexec_setup condor_glexec_run condor_glexec_cleanup condor_glexec_kill condor_glexec_update_proxy DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	condor_exe(condor_glexec_job_wrapper "glexec_job_wrapper.linux.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS}" OFF)
endif()

if (HAVE_SSH_TO_JOB)
	install ( FILES condor_ssh_to_job_sshd_setup condor_ssh_to_job_shell_setup DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/condor_ssh_to_job_sshd_config_template.in ${CMAKE_CURRENT_BINARY_DIR}/condor_ssh_to_job_sshd_config_template @ONLY )
	install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/condor_ssh_to_job_sshd_config_template DESTINATION ${C_LIB} )
endif(HAVE_SSH_TO_JOB)
