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
		file( GLOB RmvSrcs *_gahp_wrapper* *dcloud* *ec2* )
	endif()
	condor_glob( GMHDRS GMSRCS "${RmvSrcs}" )

	if ( LINUX )
		condor_exe( condor_gridmanager "${GMHDRS};${GMSRCS}" ${C_SBIN} "${GM_LINK_LIBS};${LIBUUID_FOUND}" OFF )
	else()
		condor_exe( condor_gridmanager "${GMHDRS};${GMSRCS}" ${C_SBIN} "${GM_LINK_LIBS}" OFF )
	endif()

	if (NOT WINDOWS)
        if (HAVE_EXT_GLOBUS)
            add_custom_target( grid_monitor
                ALL cat ${CMAKE_CURRENT_SOURCE_DIR}/grid_monitor_wrapper ${CMAKE_CURRENT_SOURCE_DIR}/grid_manager_monitor_agent > "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor" && chmod 755 "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor"
                DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/grid_monitor_wrapper ${CMAKE_CURRENT_SOURCE_DIR}/grid_manager_monitor_agent
            )
            install (FILES "${CMAKE_CURRENT_BINARY_DIR}/grid_monitor" DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	    # CRUFT: grid_monitor.sh was renamed to grid_monitor in
	    #   Condor 7.7.6. The default in the config file was changed as
	    #   well. But we keep a symlink for the old location for
	    #   installations upgrading from older versions. This should
	    #   be removed eventually.
	    clone_install(grid_monitor "${C_SBIN}" "grid_monitor.sh" "${C_SBIN}")
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

	install (FILES remote_gahp DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

	endif()

endif(NOT WIN_EXEC_NODE_ONLY)
