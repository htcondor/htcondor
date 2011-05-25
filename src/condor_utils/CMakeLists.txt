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
soap_helpers* test_* *.t.cpp *test.cpp cat_url.cpp *Test* param_info_init.c )

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

############ generate params
add_custom_command (OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/param_info_init.c
                    COMMAND perl
                    ARGS ${CMAKE_CURRENT_SOURCE_DIR}/param_info_c_generator.pl -i ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in -o ${CMAKE_CURRENT_BINARY_DIR}/param_info_init.c 
                    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/param_info.in ${CMAKE_CURRENT_SOURCE_DIR}/param_info_c_generator.pl)

add_custom_target (utils_genparams ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/param_info_init.c)
############ end generate params

condor_static_lib( utils "${HeaderFiles};${SourceFiles};${SAFE_OPEN_SRC}" )
add_dependencies(utils utils_genparams syscall_numbers )

##################################################
# condorapi & tests
condor_selective_glob("my_username.*;condor_event.*;file_sql.*;misc_utils.*;user_log_header.*;write_user_log*;read_user_log*;iso_dates.*;file_lock.*;format_time.*;utc_time.*;stat_wrapper*;log_rotate.*;dprintf*;sig_install.*;basename.*;mkargv.*;except.*;strupr.*;lock_file.*;rotate_file.*;strcasestr.*;strnewp.*;condor_environ.*;setsyscalls.*;passwd_cache.*;uids.c*;chomp.*;subsystem_info.*;my_subsystem.*;distribution.*;my_distribution.*;get_random_num.*;libcondorapi_stubs.*;seteuid.*;setegid.*;condor_open.*;classad_merge.*;condor_attributes.*;simple_arg.*;compat_classad.*;compat_classad_util.*;classad_oldnew.*;condor_snutils.*;stringSpace.*;string_list.*;MyString.*;condor_xml_classads.*;directory*;${SAFE_OPEN_SRC}" ApiSrcs)
if(WINDOWS)
    condor_selective_glob("directory.WINDOWS.*;directory_util.*;dynuser.WINDOWS.*;filename_tools.*;lock_file.WINDOWS.*;lsa_mgr.*;my_dynuser.*;ntsysinfo.WINDOWS.*;posix.WINDOWS.*;stat.WINDOWS.*;store_cred.*;token_cache.WINDOWS.*;truncate.WINDOWS.*" ApiSrcs)
    set_property( TARGET utils_genparams PROPERTY FOLDER "libraries" )
endif()

condor_static_lib( condorapi "${ApiSrcs}" )
add_dependencies( condorapi utils )

if (HAVE_LIBDL)
	set(APILIBDEP dl)
endif()

target_link_libraries(condorapi ${CLASSADS_FOUND};${APILIBDEP})

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
endif(WANT_FULL_DEPLOYMENT)

add_definitions(-DENABLE_STATE_DUMP)
condor_exe_test(test_log_reader "test_log_reader.cpp" "${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND}") 
condor_exe_test(test_log_reader_state "test_log_reader_state.cpp" "${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND}")
condor_exe_test(test_log_writer "test_log_writer.cpp" "${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND}")
condor_exe_test(test_libcondorapi "test_libcondorapi.cpp" "condorapi")

##################################################
# std universe stubgen stuff
if (STD_UNIVERSE)
	command_target(y.tab yacc "-d;${CMAKE_CURRENT_SOURCE_DIR}/stub_gen_yacc.y" y.tab.c)
	command_target(lex.yy lex "${CMAKE_CURRENT_SOURCE_DIR}/stub_gen_lex.l" lex.yy.c)
	condor_exe(stub_gen "lex.yy.c;y.tab.c;lex.yy.c" OFF "" OFF)
endif(STD_UNIVERSE)
##################################################

