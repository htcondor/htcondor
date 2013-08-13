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

		if (NOT CURL_FOUND)
			message(FATAL_ERROR "Can't find libcurl, needed to build libdeltacloud external!")
		endif()

		condor_pre_external( LIBDELTACLOUD libdeltacloud-0.9 "lib;include" "lib/libdeltacloud.a")

		ExternalProject_Add(libdeltacloud
				#-- Download Step ----------
				DOWNLOAD_DIR ${LIBDELTACLOUD_STAGE}/dl
				URL ${EXTERNALS_SOURCE_URL}/${LIBDELTACLOUD_VER}.tar.gz
				#--Configure step ----------
				CONFIGURE_COMMAND ./configure --prefix=${LIBDELTACLOUD_INSTALL_LOC}
				#--Build Step ----------
				BUILD_COMMAND make
				BUILD_IN_SOURCE 1
				#--install Step ----------
				INSTALL_DIR ${LIBDELTACLOUD_INSTALL_LOC}
				INSTALL_COMMAND make install )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBDELTACLOUD_FOUND "${LIBDELTACLOUD_INSTALL_LOC}/lib/libdeltacloud.a")

		condor_post_external( libdeltacloud include OFF )

	else( NOT PROPER )

		check_include_files("libdeltacloud/libdeltacloud.h" HAVE_DELTACLOUD_H)
		if (HAVE_DELTACLOUD_H)
			find_multiple( "deltacloud" LIBDELTACLOUD_FOUND )
		endif()

	endif( NOT PROPER )
endif()

if (LIBDELTACLOUD_FOUND)
	message (STATUS "external configured (LIBDELTACLOUD_FOUND=${LIBDELTACLOUD_FOUND})")
	set( LIBDELTACLOUD_FOUND ${LIBDELTACLOUD_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_LIBDELTACLOUD ON PARENT_SCOPE )
else()
	message (STATUS "external skipped (libdeltacloud)")
endif(LIBDELTACLOUD_FOUND)
