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


if(NOT WIN_EXEC_NODE_ONLY)
	set (GM_LINK_LIBS "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${COREDUMPER_FOUND}")

	if ( LINUX )
		file( GLOB RmvSrcs *_gahp_wrapper* )
	else()
		file( GLOB RmvSrcs *_gahp_wrapper* *dcloud* )
	endif()
	condor_glob( GMHDRS GMSRCS "${RmvSrcs}" )

	if ( LINUX )
		condor_exe( condor_gridmanager "${GMHDRS};${GMSRCS}" ${C_SBIN} "${GM_LINK_LIBS};${LIBUUID_FOUND}" OFF )
	else()
		condor_exe( condor_gridmanager "${GMHDRS};${GMSRCS}" ${C_SBIN} "${GM_LINK_LIBS}" OFF )
	endif()

	if (NOT WINDOWS)
        if (HAVE_EXT_GLOBUS)
            condor_exe( gt4_gahp "gt4_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" OFF )
            condor_exe( gt42_gahp "gt42_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" OFF )

            add_custom_command(
                OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor.sh"
                DEPENDS grid_monitor_wrapper grid_manager_monitor_agent
                COMMAND cat
                ARGS grid_monitor_wrapper grid_manager_monitor_agent > "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor.sh" && chmod 755 "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor.sh"
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            )
            add_custom_target( grid_monitor
                    ALL
                    DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor.sh"
            )
            install (FILES "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor.sh" DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

        endif()

        if (HAVE_EXT_UNICOREGAHP)
            condor_exe( unicore_gahp "unicore_gahp_wrapper.cpp" ${C_SBIN} "${GM_LINK_LIBS}" OFF )
        endif()

        if (HAVE_EXT_BLAHP)
            install (FILES
                nqs_cancel.sh
                nqs_hold.sh
                nqs_resume.sh
                nqs_status.sh
                nqs_submit.sh
                DESTINATION ${C_LIBEXEC}/glite/bin
                PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
        endif()

	endif()

	install (FILES gridftp_wrapper.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

endif(NOT WIN_EXEC_NODE_ONLY)
