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

	condor_pre_external( UNICOREGAHP unicoregahp-1.2.0 "usr/share/java" "usr/share/java/ugahp.jar")

	ExternalProject_Add(unicoregahp
			    #-- Download Step ----------
			    DOWNLOAD_DIR ${UNICOREGAHP_STAGE}/dl
			    URL ${EXTERNALS_SOURCE_URL}/${UNICOREGAHP_VER}.tar.gz
				#--Configure step ----------
				CONFIGURE_COMMAND echo "nothing to do"
				#--Build Step ----------
				BUILD_COMMAND echo "nothing to do"
				BUILD_IN_SOURCE 1
				#--install Step ----------
			    INSTALL_DIR ${UNICOREGAHP_INSTALL_LOC}/usr/share/java
			    INSTALL_COMMAND cp ugahp.jar ${UNICOREGAHP_INSTALL_LOC}/usr/share/java )

	message (STATUS "external configured (unicoregahp)")

	condor_post_external( unicoregahp OFF OFF "" )
	install(FILES "${UNICOREGAHP_INSTALL_LOC}/usr/share/java/ugahp.jar" DESTINATION ${C_LIB})

else()
	message (STATUS "external skipped (unicoregahp)")
endif()
