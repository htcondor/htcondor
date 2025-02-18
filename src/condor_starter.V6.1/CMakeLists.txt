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
starter_guidance.cpp
docker_proc.cpp
filter.cpp
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
execute_dir_monitor.cpp
job_info_communicator.cpp
local_user_log.cpp
NTsenders.cpp
os_proc.cpp
parallel_proc.cpp
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
		../condor_startd.V6/VolumeManager.cpp
	)
endif(LINUX)

if (UNIX)
set_source_files_properties(jic_shadow.cpp PROPERTIES COMPILE_FLAGS "-Wno-deprecated-declarations")
endif(UNIX)

condor_daemon( EXE condor_starter SOURCES "${starterElements}" LIBRARIES "${CONDOR_LIBS}" INSTALL "${C_SBIN}")

# need to add all the other install targets for libexec.
install ( FILES CondorJavaWrapper.class CondorJavaInfo.class DESTINATION ${C_LIB} )

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

# and repeat the process with an exit_37 built on an arm machine..
# and name the file htcondor_docker_test_arm

# At startup we run singularity with a sandbox dir of $(LIBEXEC)/singularity_test_sandbox
# and run this small program to verify that it exits with code 37
if (LINUX)
	add_executable(exit_37 "exit_37.c")

	# At startup, run this program which emits the current user namespace
	# if it is different from ours, the singularity we ran it in is
	# configured to use user namespaces.  Otherwise, it is a setuid singularity
	add_executable(get_user_ns "get_user_ns.c")
	target_link_libraries(get_user_ns "-static")

	# Address Sanitizer can't deal with statically linked exes
	if (WITH_ADDRESS_SANITIZER)
		set_target_properties( exit_37 PROPERTIES COMPILE_FLAGS "-fno-sanitize=address")
		set_target_properties( exit_37 PROPERTIES LINK_FLAGS "-fno-sanitize=address")
	endif()

	if (WITH_UB_SANITIZER)
		set_target_properties( exit_37 PROPERTIES COMPILE_FLAGS "-fno-sanitize=undefined")
		set_target_properties( exit_37 PROPERTIES LINK_FLAGS "-fno-sanitize=undefined")
	endif()


	# Our special assembly version of exit_37 needs special flags
	# but only works on 64 bit linux
	if ("${SYS_ARCH}" STREQUAL "X86_64")
		target_link_libraries(exit_37 "-nostdlib -static")
	else()
		target_link_libraries(exit_37 "-static")
	endif()
endif()

# Just like we can't asume that docker is installed or runnable on the build machine
# we can't assume that singularity is either, so we check in a small .sif file
# built with 
# mkdir chroot
# cp $(LIBEXEC)/singularity_test_sandbox/bin/exit_37 chroot
# singularity build exit_37.sif chroot

if (LINUX)
	install ( FILES condor_container_launcher.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/htcondor_docker_test DESTINATION ${C_LIBEXEC} )
	install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/htcondor_docker_test_arm DESTINATION ${C_LIBEXEC} )
	if ("${SYS_ARCH}" STREQUAL "AARCH64" OR "${SYS_ARCH}" STREQUAL "PPC64LE" OR "${SYS_ARCH}" STREQUAL "X86_64")
		install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/exit_37.sif.${SYS_ARCH} RENAME exit_37.sif DESTINATION ${C_LIBEXEC})
	endif()
	install ( DIRECTORY DESTINATION ${C_LIBEXEC}/singularity_test_sandbox )
	install ( DIRECTORY DESTINATION ${C_LIBEXEC}/singularity_test_sandbox/dev )
	install ( DIRECTORY DESTINATION ${C_LIBEXEC}/singularity_test_sandbox/proc )
	install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/exit_37 DESTINATION ${C_LIBEXEC}/singularity_test_sandbox PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
	install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/get_user_ns DESTINATION ${C_LIBEXEC}/singularity_test_sandbox PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
endif()


# The diagnostic tools the shadow might invoke.
condor_exe(
    condor_diagnostic_send_ep_logs
    "condor_diagnostic_send_ep_logs.cpp"
    ${C_LIBEXEC} "${CONDOR_LIBS}" OFF
)

