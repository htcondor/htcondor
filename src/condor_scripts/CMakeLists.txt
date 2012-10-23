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


if (NOT WINDOWS)

    if (HAVE_EXT_GLOBUS)
        install ( FILES condor_glidein DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
    endif()

    install ( FILES condor_run DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
    install ( FILES condor_updates_stats condor_init DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
    install ( FILES condor_gather_info DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

    if (WANT_FULL_DEPLOYMENT)
        install ( FILES condor_configure DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
        install ( FILES condor_configure DESTINATION ${C_SBIN} RENAME condor_install PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
        install ( FILES condor_configure DESTINATION ${C_SBIN} RENAME bosco_install PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
        install ( CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink ./sbin/bosco_install \${CMAKE_INSTALL_PREFIX}/bosco_install)" )
        install ( CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink ./sbin/condor_install \${CMAKE_INSTALL_PREFIX}/condor_install)" )
        install ( CODE "EXECUTE_PROCESS(COMMAND ${CMAKE_COMMAND} -E create_symlink ./sbin/condor_configure \${CMAKE_INSTALL_PREFIX}/condor_configure)" )
    endif(WANT_FULL_DEPLOYMENT)

	install ( FILES condor_ssh sshd.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

	if (STD_UNIVERSE)
		install ( FILES condor_compile DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
		install ( FILES ld DESTINATION ${C_LIB} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
		install ( FILES ld DESTINATION ${C_LIB} RENAME real-ld PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
	endif(STD_UNIVERSE)

endif (NOT WINDOWS)
