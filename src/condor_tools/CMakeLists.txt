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


condor_exe(condor_cod "cod_tool.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_preen "preen.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_testwritelog "testwritelog.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_drain "drain.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_advertise "advertise.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_ping "ping.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_fetchlog "fetch_log.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_sos "sos.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_config_val "config_val.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_userprio "user_prio.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_findhost "give_interactive.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_qedit "qedit.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
#condor_exe(condor_curlplugin "condor_curlplugin.c" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

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

include_directories(${BOOST_INCLUDE})
condor_exe(condor_tail "peek.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)



condor_exe(condor_transfer_data "transfer_data.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_version "version.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_wait "wait.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_history "history.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS};${POSTGRESQL_FOUND}" OFF)

if (WANT_QUILL AND HAVE_EXT_POSTGRESQL)
	condor_exe(condor_load_history "load_history.cpp" ${C_BIN} "tt;${CONDOR_TOOL_LIBS};${POSTGRESQL_FOUND}" OFF)
	condor_exe(condor_dump_history "dump_history.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS};${POSTGRESQL_FOUND}" OFF)
	condor_exe(condor_convert_history "convert_history.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS};${POSTGRESQL_FOUND}" OFF)
endif(WANT_QUILL AND HAVE_EXT_POSTGRESQL)

condor_exe(condor_store_cred "store_cred_main.cpp" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF)

condor_exe(condor_vacate "tool.cpp" "${C_BIN}" "${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_on "tool.cpp" "${C_SBIN}" "${CONDOR_TOOL_LIBS}" OFF)

clone_install(condor_on "${C_SBIN}" "condor_off;condor_restart;condor_reconfig;condor_set_shutdown" "${C_SBIN}")
if (WANT_FULL_DEPLOYMENT)
    clone_install(condor_on "${C_SBIN}" "condor_set_shutdown" "${C_SBIN}")
endif(WANT_FULL_DEPLOYMENT)
clone_install(condor_vacate "${C_BIN}" "condor_reschedule" "${C_BIN}")

if (NOT WINDOWS)
	if (STD_UNIVERSE)
		clone_install(condor_vacate "${C_BIN}" "condor_checkpoint" "${C_BIN}")
		src_target_ref( ckpt "maps" CKPT_REFS)
		# condor_exe(condor_ckpt_probe "probe_process.cpp;${CKPT_REFS}" "${C_LIBEXEC}" "${CONDOR_TOOL_LIBS}" OFF)
		#
		# Statically link condor_ckpt_probe; see #2901.
		#
		# This is wasteful, since we've already built three of these source
		# files, but it's a lot simpler than figuring out how to link the
		# object files.
		set( _orig_CMAKE_FIND_LIBRARY_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES} )
		set( CMAKE_FIND_LIBRARY_SUFFIXES .a )
		find_library( HAVE_STATIC_LIBDL dl )
		set( CMAKE_FIND_LIBRARY_SUFFIXES ${_orig_CMAKE_FIND_LIBRARY_SUFFIXES} )
		if (NOT HAVE_STATIC_LIBDL)
			message( FATAL_ERROR "Can't find libdl.a, needed for condor_ckpt_probe" )
		endif()
		add_executable( condor_ckpt_probe probe_process.cpp ../condor_ckpt/maps.c ../condor_utils/condor_pers.cpp ../safefile/safe_open.cpp )
		install( TARGETS condor_ckpt_probe DESTINATION "${C_LIBEXEC}" )
        set_property( TARGET condor_ckpt_probe PROPERTY COMPILE_FLAGS -static )
        set_property( TARGET condor_ckpt_probe PROPERTY LINK_FLAGS -static )
	endif(STD_UNIVERSE)
else(NOT WINDOWS)
	clone_install(condor_cod "${C_BIN}" "condor_cod_request" "${C_BIN}")
	clone_install(condor_vacate "${C_BIN}" "condor_squawk" "${C_BIN}")
endif(NOT WINDOWS)

condor_exe(condor_stats "stats.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

if( WINDOWS )
	if (MSVC11)
		condor_exe(condor_setup "condor_setup.c;soap_setupStub.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
	else()
		condor_exe(condor_setup "condor_setup.c" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
	endif()
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
		 COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
		 )" )
	# This is required for opensuse, where the system's libssl and
	# libcrypto files don't have owner write permission.
	install( CODE "execute_process(COMMAND chmod -R u+w ${CMAKE_INSTALL_PREFIX}/lib/condor/)" )
endif()

if (CURL_FOUND)
  if (WINDOWS)
    condor_exe( config_fetch "file_fetch.cpp" ${C_LIBEXEC} "${CURL_FOUND};wldap32.lib;${CONDOR_TOOL_LIBS}" OFF )
  else()
    condor_exe( config_fetch "file_fetch.cpp" ${C_LIBEXEC} "${CURL_FOUND};${CONDOR_TOOL_LIBS}" OFF )
  endif()
endif()

############################################################
# GPU Support
############################################################
if ( LINUX AND HAVE_LIBDL AND HAVE_DLFCN_H AND HAVE_DLOPEN )
	condor_exe( condor_gpu_discovery "condor_gpu_discovery.cpp" ${C_LIBEXEC} "${HAVE_LIBDL}" OFF )
else ()
	if (WINDOWS)
		condor_exe( condor_gpu_discovery "condor_gpu_discovery.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS}" OFF )
	endif ()
endif()
