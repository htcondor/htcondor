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


##################################################
# utils library
file( GLOB RmvSrcs y.tab* lex.yy* ckpt_server_api_stubs* libcondorapi_stubs* 
soap_helpers* test_* *.t.cpp *test.cpp cat_url.cpp *Test* param_info_hash.cpp )

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

############ generate params
add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h
		COMMAND perl
		ARGS ${CMAKE_CURRENT_SOURCE_DIR}/param_table_generator.pl -i ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in -o ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in ${CMAKE_CURRENT_SOURCE_DIR}/param_table_generator.pl)

add_custom_target (utils_genparams ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/param_info_tables.h)

if(UNIX)
	set_source_files_properties(param_info.cpp PROPERTIES COMPILE_FLAGS -Wno-unused-parameter)
endif(UNIX)

############ end generate params

# These are source files that reside in other directories that we want to
# include in libcondor_utils. They used to be in separate libraries.
set( EXTRA_SRCS "${SAFE_OPEN_SRC};${ANALYSIS_UTIL_SRCS};${CCB_UTIL_SRCS};${CEDAR_UTIL_SRCS};${CKPT_SERVER_UTIL_SRCS};${DAEMON_CLIENT_UTIL_SRCS};${DAEMON_CORE_UTIL_SRCS};${PRIVSEP_UTIL_SRCS};${PROCAPI_UTIL_SRCS};${PROCD_CLIENT_UTIL_SRCS};${QMGMT_UTIL_SRCS};${SYSAPI_UTIL_SRCS}" )
if (UNIX)
set_source_files_properties(../condor_sysapi/clinpack.cpp PROPERTIES COMPILE_FLAGS -w)
set_source_files_properties(../condor_daemon_core.V6/self_monitor.cpp ../classad_analysis/analysis.cpp ../classad_analysis/interval.cpp generic_stats.cpp generic_query.cpp PROPERTIES COMPILE_FLAGS -Wno-float-equal)
endif(UNIX)

# Turn off the OpenSSL deprecation warnings until we decide on the long-term direction with OpenSSL on Mac.
if (DARWIN)
	set_source_files_properties(condor_base64.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(condor_dh.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(condor_md.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_auth_passwd.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_auth_ssl.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt_3des.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
	set_source_files_properties(../condor_io/condor_crypt_blowfish.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated-declarations)
endif()

#On windows we add the cpp file into the build 
if (GSOAP_SRC)
	set_source_files_properties(${GSOAP_SRC} PROPERTIES GENERATED TRUE)
	list(APPEND EXTRA_SRCS ${GSOAP_SRC} )
endif()

condor_shared_lib( condor_utils "${HeaderFiles};${SourceFiles};${EXTRA_SRCS}" )
add_dependencies(condor_utils utils_genparams syscall_numbers )
if ( ${PACKAGE_VERSION} MATCHES "([0-9]+)[.]([0-9]+)[.]([0-9]+)" )
	set( UTILS_LIB_NAME "condor_utils_${CMAKE_MATCH_1}_${CMAKE_MATCH_2}_${CMAKE_MATCH_3}" )
else()
	message(FATAL_ERROR "Can't determine Condor version!")
endif()
set_target_properties( condor_utils PROPERTIES OUTPUT_NAME "${UTILS_LIB_NAME}" )

# for linux also make a static version of condor_utils
#
if (LINUX OR DARWIN)
   condor_static_lib( condor_utils_s "${HeaderFiles};${SourceFiles};${EXTRA_SRCS}" )
   add_dependencies( condor_utils_s utils_genparams syscall_numbers )
   set_target_properties( condor_utils_s PROPERTIES OUTPUT_NAME "${UTILS_LIB_NAME}" )
endif(LINUX OR DARWIN)


# Brain twister:
#
# If you dynamicly load a library which links against libcondor_utils (such as
# a python module), the dprintf inside libcondor_utils will be linked against
# the libc dprintf.  Hilarity ensues as internal condor functions use the libc
# dprintf.
#
# The magic below will cause internal references to dprintf to be changed to
# __wrap_dprintf, which is also provided by condor_utils.  Hence, when
# dynamically loaded, Condor's __wrap_dprintf (identical to Condor's dprintf)
# is used instead of libc's.  Condor's dprintf is still exported, meaning
# linking executables against condor_utils still works.
#
# Voodoo, that's what this is.  Never overload libc functions!
#
if ( LINUX )
  set_target_properties( condor_utils PROPERTIES LINK_FLAGS "-Wl,--wrap=dprintf" )
endif()

target_link_libraries(condor_utils ${CLASSADS_FOUND} ${PCRE_FOUND} ${VOMS_FOUND} ${GLOBUS_FOUND} ${OPENSSL_FOUND} ${KRB5_FOUND} ${POSTGRESQL_FOUND} ${COREDUMPER_FOUND} )
if ( DARWIN )
	target_link_libraries( condor_utils ${IOKIT_FOUND} ${COREFOUNDATION_FOUND} resolv )
	add_custom_command( TARGET condor_utils
		POST_BUILD
		COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs lib${UTILS_LIB_NAME}.dylib )
endif()
if(WINDOWS)
	add_dependencies(condor_utils utils_genparams syscall_numbers )
else()
	add_dependencies(condor_utils utils_genparams syscall_numbers configure_safefile)
endif()

##################################################
# condorapi & tests

condor_selective_glob("my_username.*;condor_event.*;file_sql.*;misc_utils.*;user_log_header.*;write_user_log*;read_user_log*;iso_dates.*;file_lock.*;format_time.*;utc_time.*;stat_wrapper*;log_rotate.*;dprintf.cpp;dprintf_c*;dprintf_setup.cpp;sig_install.*;basename.*;mkargv.*;except.*;strupr.*;lock_file.*;rotate_file.*;strcasestr.*;strnewp.*;condor_environ.*;setsyscalls.*;passwd_cache.*;uids.c*;chomp.*;subsystem_info.*;my_subsystem.*;distribution.*;my_distribution.*;get_random_num.*;libcondorapi_stubs.*;seteuid.*;setegid.*;condor_open.*;classad_merge.*;condor_attributes.*;simple_arg.*;compat_classad.*;compat_classad_util.*;classad_oldnew.*;condor_snutils.*;stringSpace.*;string_list.*;stl_string_utils.*;MyString.*;condor_xml_classads.*;directory*;param_functions.*;filename_tools_cpp.*;filename_tools.*;stat_info.*;${SAFE_OPEN_SRC}" ApiSrcs)
if(WINDOWS)
    condor_selective_glob("directory.WINDOWS.*;directory_util.*;dynuser.WINDOWS.*;lock_file.WINDOWS.*;lsa_mgr.*;my_dynuser.*;ntsysinfo.WINDOWS.*;posix.WINDOWS.*;stat.WINDOWS.*;store_cred.*;token_cache.WINDOWS.*;truncate.WINDOWS.*" ApiSrcs)
    set_property( TARGET utils_genparams PROPERTY FOLDER "libraries" )
else()
    condor_selective_glob("dprintf_syslog*" ApiSrcs)
endif()
add_subdirectory(tests)

condor_static_lib( condorapi "${ApiSrcs}" )
add_dependencies( condorapi condor_utils )

if (HAVE_LIBDL)
	set(APILIBDEP dl)
endif()

target_link_libraries(condorapi ${CLASSADS_FOUND};${APILIBDEP})

if (LINUX OR DARWIN)
	condor_shared_lib( condorapi_shared "${ApiSrcs}" )
	add_dependencies( condorapi_shared condor_utils )
	target_link_libraries(condorapi_shared ${CLASSADS_FOUND};${APILIBDEP})
	set_target_properties( condorapi_shared PROPERTIES OUTPUT_NAME condorapi )
endif()

if (WANT_FULL_DEPLOYMENT)
	install (FILES MyString.h
			 condor_event.h
			 condor_holdcodes.h
			 read_user_log.h
			 user_log.c++.h
			 user_log.README
			 write_user_log.h
			 compat_classad.h
			 compat_classad_list.h
			 compat_classad_util.h
			 stl_string_utils.h
			DESTINATION ${C_INCLUDE})

    install ( TARGETS condorapi DESTINATION ${C_LIB} )
    if (LINUX OR DARWIN)
        install ( TARGETS condorapi_shared DESTINATION ${C_LIB} )
    endif()
endif(WANT_FULL_DEPLOYMENT)

add_definitions(-DENABLE_STATE_DUMP)
condor_exe_test(test_log_reader "test_log_reader.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_log_reader_state "test_log_reader_state.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_log_writer "test_log_writer.cpp" "${CONDOR_TOOL_LIBS}")
condor_exe_test(test_libcondorapi "test_libcondorapi.cpp" "condorapi")

##################################################
# std universe stubgen stuff
if (STD_UNIVERSE)
	command_target(y.tab yacc "-d;${CMAKE_CURRENT_SOURCE_DIR}/stub_gen_yacc.y" y.tab.c)
	command_target(lex.yy lex "${CMAKE_CURRENT_SOURCE_DIR}/stub_gen_lex.l" lex.yy.c)
	condor_exe(stub_gen "lex.yy.c;y.tab.c;lex.yy.c" OFF "" OFF)
endif(STD_UNIVERSE)
##################################################

