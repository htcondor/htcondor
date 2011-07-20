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

		condor_pre_external( LIBDELTACLOUD libdeltacloud-0.9 "lib;include" "lib/libdeltacloud.a")

		if ( BUILDING_LIBXML2 )
			set (LIBDELTACLOUD_DEPENDS ${CURL_REF} ${LIBXML2_REF})
			set (LIBDELTACLOUD_CONFIGURE --with-libxml=${LIBXML2_INSTALL_LOC})
		else ()
			set (LIBDELTACLOUD_DEPENDS ${CURL_REF})
			set (LIBDELTACLOUD_CONFIGURE "")
		endif ()
		ExternalProject_Add(libdeltacloud
				DEPENDS ${LIBDELTACLOUD_DEPENDS}
				#-- Download Step ----------
				DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${LIBDELTACLOUD_VER}.tar.gz ${CMD_TERM}
				DOWNLOAD_DIR ${LIBDELTACLOUD_STAGE}/dl
				URL http://parrot.cs.wisc.edu/externals/${LIBDELTACLOUD_VER}.tar.gz
				#--Configure step ----------
				CONFIGURE_COMMAND cd ${LIBDELTACLOUD_VER} && ./configure --prefix=${LIBDELTACLOUD_INSTALL_LOC} --with-libcurl=${CURL_INSTALL_LOC} --disable-shared ${LIBDELTACLOUD_CONFIGURE}
				#--Build Step ----------
				BUILD_COMMAND cd ${LIBDELTACLOUD_VER} && make
				BUILD_IN_SOURCE 1
				#--install Step ----------
				INSTALL_DIR ${LIBDELTACLOUD_INSTALL_LOC}
				INSTALL_COMMAND cd ${LIBDELTACLOUD_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBDELTACLOUD_FOUND "${LIBDELTACLOUD_INSTALL_LOC}/lib/libdeltacloud.a")

		condor_post_external( libdeltacloud include OFF )

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
