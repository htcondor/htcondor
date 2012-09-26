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

install(FILES		condor.sysconfig
		DESTINATION	"${C_SYSCONFIG}"
		PERMISSIONS ${CONDOR_SCRIPT_PERMS}
		RENAME		condor)

install(FILES		condor_config.local.generic
		DESTINATION	"${C_ETC}"
		RENAME		condor_config.local)

install (FILES
		condor.boot.generic
		condor_config.local.central.manager
		condor_config.local.ha_condor_client
		condor.sysconfig
		README
		condor.boot.rpm
		condor_config.local.ckpt.server
		condor_config.local.ha_condor_had
		condor_config.generic
		condor_config.generic.redhat
		condor_config.local.dedicated.resource
		condor_config.local.view_server
		#condor_config.generic.debian.patch
		condor_config.local.dedicated.submit
		condor_config.submit.generic
		#condor_config.generic.rpm.patch
		#condor_config.local.generic
		condor.init
		condor.plist
		DESTINATION ${C_ETC_EXAMPLES})

install (FILES
	  openmpiscript
	  mp2script
	  mp1script
	  lamscript
	 DESTINATION ${C_ETC_EXAMPLES}
	 PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

install (FILES 
	condor_limits_wrapper.sh
	condor_schedd.init
	interactive.sub
	DESTINATION ${C_LIBEXEC}
	PERMISSIONS ${CONDOR_SCRIPT_PERMS})

#Prepare OS specific config file

if ( RPM_SYSTEM_NAME AND CONDOR_PACKAGE_BUILD)


	if (${BIT_MODE} MATCHES "32" OR ${SYS_ARCH} MATCHES "IA64" )	
		add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
						COMMAND cp 
						ARGS ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic condor_config.patched
						COMMAND patch 
						ARGS condor_config.patched ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic.rpm.patch )

		add_custom_target(	patch_configurations
						ALL
						DEPENDS condor_config.patched)
		install(FILES		${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
				DESTINATION	"${C_ETC}"
				RENAME	condor_config)
	else()
		#x86_64 use /usr/lib64 instead of /usr/lib
		#we will apply 2 patches on condor_config file
		add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
						COMMAND cp 
						ARGS ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic condor_config.patched
						COMMAND patch 
						ARGS condor_config.patched ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic.rpm.patch )

		add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched.x86_64
						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
						COMMAND cp 
						ARGS ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched condor_config.patched.x86_64
						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
						COMMAND patch 
						ARGS condor_config.patched.x86_64 ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic.rpm64.patch )
	
		add_custom_target(	patch_configurations_x86_64
						ALL
						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched.x86_64)
		
		install(FILES	${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched.x86_64
						DESTINATION	"${C_ETC}"
						RENAME	condor_config)					
	endif ()					

	install(FILES		condor.boot.rpm
			DESTINATION	"${C_INIT}"
			PERMISSIONS ${CONDOR_SCRIPT_PERMS}
			RENAME		condor)

elseif ( DEB_SYSTEM_NAME AND CONDOR_PACKAGE_BUILD)

	add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
						COMMAND cp
						ARGS ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic condor_config.patched
					    COMMAND patch
						ARGS condor_config.patched ${CMAKE_CURRENT_SOURCE_DIR}/condor_config.generic.debian.patch )

	add_custom_target(	patch_configurations
						ALL
						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched)

	install(FILES		${CMAKE_CURRENT_BINARY_DIR}/condor_config.patched
						DESTINATION	"${C_ETC}"
						RENAME	condor_config)

	#Debian init script need to be patched because /etc/sysconfig/condor -> /etc/default/condor
	add_custom_command( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor.boot.debian
						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
						COMMAND cp
						ARGS ${CMAKE_CURRENT_SOURCE_DIR}/condor.boot.rpm condor.boot.debian
						COMMAND patch
						ARGS condor.boot.debian ${CMAKE_CURRENT_SOURCE_DIR}/condor.boot.debian.patch )

	add_custom_target(	patch_init_script
						ALL
						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/condor.boot.debian)

	install(FILES		${CMAKE_CURRENT_BINARY_DIR}/condor.boot.debian
			DESTINATION	"${C_INIT}"
			PERMISSIONS ${CONDOR_SCRIPT_PERMS}
			RENAME	condor)
else()
	#Other Platform
	install(FILES		condor_config.generic
			DESTINATION	"${C_ETC}"
			RENAME	condor_config)

	install(FILES		condor.boot.rpm
			DESTINATION	"${C_INIT}"
			PERMISSIONS ${CONDOR_SCRIPT_PERMS}
			RENAME		condor)

endif()
else()
						 
    add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/condor_config.win
						 WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
						 COMMAND awk
						 ARGS -f convert_config_to_win32.awk condor_config.generic > "${CMAKE_CURRENT_BINARY_DIR}/condor_config.win"
					   )
						 
	add_custom_target(	patch_configurations
						ALL
						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/condor_config.win)
						
	install(FILES		${CMAKE_CURRENT_BINARY_DIR}/condor_config.win
			DESTINATION	"${C_ETC}"
			RENAME		condor_config.generic
	        )
			
	install(FILES
	        condor_config.power
	        condor_config.submit.generic
	        condor_config.local.generic
	        condor_config.local.credd 
	        condor_config.local.central.manager
	        convert_config_to_win32.awk
			DESTINATION	"${C_ETC}")


endif(NOT WINDOWS)
