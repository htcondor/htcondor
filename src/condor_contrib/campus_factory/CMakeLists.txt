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

if ( LINUX OR APPLE )
	option(WITH_CAMPUSFACTORY "Compiling with support for Campus Factory" ON)
endif ( LINUX OR APPLE )

if ( WITH_CAMPUSFACTORY )

	install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python-lib DESTINATION ${C_LIBEXEC}/campus_factory USE_SOURCE_PERMISSIONS )
        install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share DESTINATION ${C_LIBEXEC}/campus_factory USE_SOURCE_PERMISSIONS )
        install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/bin/campus_factory 
                        ${CMAKE_CURRENT_SOURCE_DIR}/bin/runfactory 
                        ${CMAKE_CURRENT_SOURCE_DIR}/bin/glidein_creation 
                        DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS}) 
        install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/etc/campus_factory.conf DESTINATION ${C_LIBEXEC}/campus_factory/etc )
        install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/share/condor/condor_config.factory DESTINATION ${C_ETC_EXAMPLES} )
		
endif( WITH_CAMPUSFACTORY )
