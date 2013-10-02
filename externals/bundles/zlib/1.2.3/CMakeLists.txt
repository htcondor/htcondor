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

if ( NOT CLIPPED )

	condor_pre_external( ZLIB  zlib-1.2.3 "lib;include" "lib/libz.a")

	ExternalProject_Add(zlib
			    #-- Download Step ----------
			    DOWNLOAD_DIR ${ZLIB_STAGE}/dl
			    URL ${EXTERNALS_SOURCE_URL}/${ZLIB_VER}.tar.gz
			    #--Configure step ----------
			    CONFIGURE_COMMAND ./configure --prefix=${ZLIB_INSTALL_LOC}
			    #--Build Step ----------
			    BUILD_COMMAND make
			    BUILD_IN_SOURCE 1
			    #--install Step ----------
			    INSTALL_DIR ${ZLIB_INSTALL_LOC}
			    INSTALL_COMMAND make install )

	condor_post_external( zlib include OFF )

	install ( FILES "${ZLIB_INSTALL_LOC}/lib/libz.a" DESTINATION ${C_LIB} RENAME libcondor_z.a )

	message (STATUS "external configured (zlib)")

else( NOT CLIPPED )

	message (STATUS "external skipped (zlib)")

endif( NOT CLIPPED )
