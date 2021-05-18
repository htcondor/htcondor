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

set( starterElements
starter.cpp
cgroup_limits.cpp
cgroup.linux.cpp
docker_proc.cpp
find_child_proc.cpp
has_sysadmin_cap.cpp
io_proxy.cpp
io_proxy_handler.cpp
java_detect.cpp
java_proc.cpp
jic_local_config.cpp
jic_local.cpp
jic_local_file.cpp
jic_local_schedd.cpp
jic_shadow.cpp
job_info_communicator.cpp
local_user_log.cpp
mpi_comrade_proc.cpp
mpi_master_proc.cpp
NTsenders.cpp
os_proc.cpp
parallel_proc.cpp
privsep_helper.cpp
remote_proc.cpp
script_proc.cpp
singularity.cpp
sshd_proc.cpp
StarterHookMgr.cpp
starter_user_policy.cpp
starter_util.cpp
starter_v61_main.cpp
stream_handler.cpp
tool_daemon_proc.cpp
user_proc.cpp
vanilla_proc.cpp
vm_gahp_request.cpp
vm_gahp_server.cpp
vm_proc.cpp
)

if (LINUX)
	list(APPEND starterElements
		cgroup.linux.cpp  
		glexec_privsep_helper.linux.cpp
	)
endif(LINUX)

if (UNIX)
set_source_files_properties(jic_shadow.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
endif(UNIX)

condor_daemon( EXE condor_starter SOURCES "${starterElements}" LIBRARIES "${CONDOR_LIBS};${LIBCGROUP_FOUND}" INSTALL "${C_SBIN}")

if (HAVE_EXT_GLOBUS)
	clone_install( condor_starter "${C_SBIN}" "condor_gridshell" "${C_SBIN}" )
endif()

# need to add all the other install targets for libexec.
install ( FILES CondorJavaWrapper.class CondorJavaInfo.class DESTINATION ${C_LIB} )

if(LINUX AND WANT_GLEXEC)
	install (FILES condor_glexec_setup condor_glexec_run condor_glexec_cleanup condor_glexec_kill condor_glexec_update_proxy DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	condor_exe(condor_glexec_job_wrapper "glexec_job_wrapper.linux.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS}" OFF)
endif()

if (HAVE_SSH_TO_JOB)
	install ( FILES condor_ssh_to_job_sshd_setup condor_ssh_to_job_shell_setup DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	configure_file( ${CMAKE_CURRENT_SOURCE_DIR}/condor_ssh_to_job_sshd_config_template.in ${CMAKE_CURRENT_BINARY_DIR}/condor_ssh_to_job_sshd_config_template @ONLY )
	install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/condor_ssh_to_job_sshd_config_template DESTINATION ${C_LIB} )
endif(HAVE_SSH_TO_JOB)

if (LINUX)
	condor_exe(condor_pid_ns_init "condor_pid_ns_init.cpp" ${C_LIBEXEC} "" OFF)
endif()
if (DOCKER_ALLOW_RUN_AS_ROOT)
	set_source_files_properties(docker_proc.cpp PROPERTIES COMPILE_DEFINITIONS DOCKER_ALLOW_RUN_AS_ROOT=1)
endif()

# We can't assume that docker is installed or runnable at build time.  So, we check in
# a docker test image.  It can be built as follows

# gcc -static -o exit_37 exit_37.c
# docker build -t htcondor_docker_test .
# docker save htcondor_docker_test > htcondor_docker_test

if (LINUX)
	install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/htcondor_docker_test DESTINATION ${C_LIBEXEC} )
endif()
