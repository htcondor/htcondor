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

if (LINUX OR DARWIN)
  option(WITH_EXPAT "Compiling with support for EXPAT" ON)
endif(LINUX OR DARWIN)

if ( WITH_EXPAT )

	if( NOT PROPER )

		set ( EXPAT_VER expat-2.0.1 )

		ExternalProject_Add(expat
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${EXPAT_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${EXPAT_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${EXPAT_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd ${EXPAT_VER} && ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${EXPAT_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd ${EXPAT_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set(EXPAT_FOUND "${EXTERNAL_STAGE}/lib/libexpat.a")
		append_var (CONDOR_EXTERNALS expat)

	else( NOT PROPER )

		find_multiple( "expat" EXPAT_FOUND )

	endif( NOT PROPER )

	if (EXPAT_FOUND)
		message (STATUS "external configured (EXPAT_FOUND=${EXPAT_FOUND})")
		set( EXPAT_FOUND ${EXPAT_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_EXPAT ON PARENT_SCOPE )
	endif (EXPAT_FOUND)

else(WITH_EXPAT)

	message (STATUS "external skipped (expat)")

endif(WITH_EXPAT)
