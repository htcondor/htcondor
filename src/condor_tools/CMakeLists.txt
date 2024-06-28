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

condor_exe(condor_manifest "manifest.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_evicted_files "evicted_files.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_now "now.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_update_machine_ad "update_machine_ad.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_preen "preen.cpp" ${C_SBIN} "${CONDOR_LIBS}" OFF)
condor_exe(condor_testwritelog "testwritelog.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_drain "drain.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_advertise "advertise.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_ping "ping.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_fetchlog "fetch_log.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_sos "sos.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_config_val "config_val.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_userprio "user_prio.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_transform_ads "transform_ads.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_findhost "give_interactive.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_qedit "qedit.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_qusers "qusers.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_create "token_create.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_fetch "token_fetch.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_list "token_list.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_request "token_request.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_request_approve "token_request_approve.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_request_list "token_request_list.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_token_request_auto_approve "token_request_auto_approve.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_scitoken_exchange "scitoken_exchange.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_ssl_fingerprint "ssl_fingerprint.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_login "login.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)

if (LINUX)
    condor_exe(condor_docker_enter "docker_enter.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
    condor_exe(condor_nsenter "nsenter.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
endif()

if (CURL_FOUND)
	if (WINDOWS)
		condor_exe( condor_urlfetch "condor_urlfetch.cpp" ${C_LIBEXEC} "${CURL_FOUND};wldap32.lib;${CONDOR_TOOL_LIBS}" OFF )
	else ()
		condor_exe( condor_urlfetch "condor_urlfetch.cpp" ${C_LIBEXEC} "${CURL_FOUND}" OFF )
	endif ()
endif()


if (HAVE_SSH_TO_JOB)
	condor_exe(condor_ssh_to_job "ssh_to_job.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
endif(HAVE_SSH_TO_JOB)

condor_exe(condor_tail "tail.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

condor_exe(condor_transfer_data "transfer_data.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_version "version.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_wait "wait.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_history "history.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_store_cred "store_cred_main.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_on "tool.cpp" "${C_SBIN}" "${CONDOR_TOOL_LIBS}" OFF)

clone_install(condor_on "${C_SBIN}" "condor_off;condor_restart;condor_reconfig;condor_set_shutdown" "${C_SBIN}")
clone_install(condor_on "${C_SBIN}" "condor_vacate;condor_reschedule" "${C_BIN}")

condor_exe(condor_stats "stats.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

if( WINDOWS )
	# condor_exe(condor_setup "condor_setup.c" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
	condor_exe(win_install_helper "win_install_helper.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
	condor_exe(win_install_probe "win_install_probe.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
	condor_exe(condor_set_acls "condor_set_acls.c" ${C_BIN} "" OFF)
endif()

condor_exe_test(condor_test_auth "test_auth.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe(condor_test_match "condor_test_match.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

# Check condor_version's shared library dependencies and copy a specific
# subset of those libraries into <release>/lib/condor
if ( LINUX AND NOT CONDOR_PACKAGE_BUILD )
	set( SRC_DIR ${CMAKE_SOURCE_DIR} )
	set( BIN_DIR ${CMAKE_BINARY_DIR} )
	install( CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/condor)" )
	install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${BIN_DIR}/src/condor_tools/condor_version ${SRC_DIR}/src/condor_scripts/syslib-patterns
	COMMAND xargs -isrc ${CMAKE_COMMAND} -E copy_if_different src ${CMAKE_INSTALL_PREFIX}/lib/condor
		 )" )
	# This is required for opensuse, where the system's libssl and
	# libcrypto files don't have owner write permission.
	install( CODE "execute_process(COMMAND chmod -R u+w ${CMAKE_INSTALL_PREFIX}/lib/condor/)" )
endif()


###########################################################
# condor_ssh_to_job hackery
##########################################################
if ( LINUX )
condor_shared_lib( libgetpwnam "getpwnam.cpp" )
set_target_properties( libgetpwnam PROPERTIES OUTPUT_NAME "getpwnam" )
endif()

# htcondor tool + Python htcondor_cli module
install ( FILES htcondor DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
install ( DIRECTORY htcondor_cli DESTINATION ${C_PYTHON3ARCHLIB} )
