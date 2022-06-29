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

install(FILES		condor-annex-ec2
		DESTINATION	"${C_ETC_EXAMPLES}"
		PERMISSIONS ${CONDOR_SCRIPT_PERMS}
)

# Only install the master shutdown script on RHEL for now
if (RPM_SYSTEM_NAME)
install(FILES		master_shutdown_script.sh
		DESTINATION	"${C_CONFIGD}/.."
		PERMISSIONS	${CONDOR_SCRIPT_PERMS}
)
endif (RPM_SYSTEM_NAME)

# Only install the small shadow configuration on RHEL
if (RPM_SYSTEM_NAME AND (${BIT_MODE} MATCHES "32"))
install(FILES		00-small-shadow
		DESTINATION	"${C_ETC_EXAMPLES}"
)
endif ()


install (FILES
		condor.boot.generic
		condor_config.local.central.manager
		condor_config.local.ha_condor_client
		README
		condor.boot.rpm
		condor_config.local.ha_condor_had
		condor_config.generic
		condor_config.tarball
		condor_config.local.dedicated.resource
		condor_config.local.view_server
		#condor_config.generic.debian.patch
		condor_config.local.dedicated.submit
		condor_config.submit.generic
		#condor_config.generic.rpm.patch
		#condor_config.local.generic
		condor_config.local.adstash
		condor.init
		condor.plist
		condor_config.annotated
		condor.service
		condor.socket
		condor-tmpfiles.conf
		condor-annex-ec2.service
		00-minicondor
		00-htcondor-9.0.config
		50-security
		50ec2.config
		DESTINATION ${C_ETC_EXAMPLES})

install (FILES
	  openmpiscript
	  mp2script
	  mp1script
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

elseif( APPLE )

	install(FILES         condor_config.tarball
	        DESTINATION   "${C_ETC}"
	        RENAME        condor_config)

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
	install(FILES		condor_config.windows.base
			DESTINATION	"${C_ETC}"
			RENAME	condor_config.base)

	install(FILES
	        condor_config.power
	        condor_config.submit.generic
	        condor_config.local.blank
	        condor_config.local.generic
	        condor_config.local.credd 
	        condor_config.local.central.manager
	        convert_config_to_win32.awk
			DESTINATION	"${C_ETC}")


endif(NOT WINDOWS)
