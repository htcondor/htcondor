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

if ( LINUX )
	option(WITH_LIBDELTACLOUD "Compiling with support for Deltacloud" ON)
endif( LINUX )

if (WITH_LIBDELTACLOUD)

	if ( NOT PROPER )

		set(LIBDELTACLOUD_VER libdeltacloud-0.6)

		ExternalProject_Add(libdeltacloud
				DEPENDS curl libxml2
				PREFIX ${EXTERNAL_BUILD_PREFIX}/${LIBDELTACLOUD_VER}
				#-- Download Step ----------
				DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${LIBDELTACLOUD_VER}.tar.gz ${CMD_TERM}
				DOWNLOAD_DIR ${EXTERNAL_DL}
				URL http://parrot.cs.wisc.edu/externals/${LIBDELTACLOUD_VER}.tar.gz
				#--Configure step ----------
			    	CONFIGURE_COMMAND cd ${LIBDELTACLOUD_VER} && ./configure --prefix=${EXTERNAL_STAGE} --with-libxml=${EXTERNAL_STAGE} --with-libcurl=${EXTERNAL_STAGE} --disable-shared
				#--Build Step ----------
				BUILD_COMMAND cd ${LIBDELTACLOUD_VER} && make
				BUILD_IN_SOURCE 1
				#--install Step ----------
				INSTALL_DIR ${EXTERNAL_STAGE}
				INSTALL_COMMAND cd ${LIBDELTACLOUD_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBDELTACLOUD_FOUND "${EXTERNAL_STAGE}/lib/libdeltacloud.a")
		append_var( CONDOR_EXTERNALS libdeltacloud )

	else( NOT PROPER )

		find_multiple( "deltacloud" LIBDELTACLOUD_FOUND )

	endif( NOT PROPER )

	if (LIBDELTACLOUD_FOUND)
		message (STATUS "external configured (LIBDELTACLOUD_FOUND=${LIBDELTACLOUD_FOUND})")
		set( LIBDELTACLOUD_FOUND ${LIBDELTACLOUD_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_LIBDELTACLOUD ON PARENT_SCOPE )
	endif(LIBDELTACLOUD_FOUND)

else(WITH_LIBDELTACLOUD)

	message (STATUS "external skipped (libdeltacloud)")

endif(WITH_LIBDELTACLOUD)
