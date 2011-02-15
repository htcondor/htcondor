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

option(WITH_UNICOREGAHP "Compiling with support for UNICOREGAHP" ON)

if ( WITH_UNICOREGAHP AND NOT PROPER )

	set (UCGAHP_VER unicoregahp-1.2.0)
	ExternalProject_Add(unicoregahp
			    PREFIX ${EXTERNAL_BUILD_PREFIX}/${UCGAHP_VER}
			    #-- Download Step ----------
				DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${UCGAHP_VER}.tar.gz ${CMD_TERM}
			    DOWNLOAD_DIR ${EXTERNAL_DL}
			    URL http://parrot.cs.wisc.edu/externals/${UCGAHP_VER}.tar.gz
				#--Configure step ----------
				CONFIGURE_COMMAND echo "nothing to do"
				#--Build Step ----------
				BUILD_COMMAND echo "nothing to do"
				BUILD_IN_SOURCE 1
				#--install Step ----------
			    INSTALL_DIR ${EXTERNAL_STAGE}/usr/share/java
			    INSTALL_COMMAND cp ugahp.jar ${EXTERNAL_STAGE}/usr/share/java )

	message (STATUS "external configured (unicoregahp)")
	set( HAVE_EXT_UNICOREGAHP ON PARENT_SCOPE )
	append_var(CONDOR_EXTERNALS unicoregahp)
	install(FILES ${EXTERNAL_STAGE}/usr/share/java/ugahp.jar DESTINATION ${C_LIB})

else()
	message (STATUS "external skipped (unicoregahp)")
endif()
