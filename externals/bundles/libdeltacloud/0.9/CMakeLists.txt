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

	check_include_files("libdeltacloud/libdeltacloud.h" HAVE_DELTACLOUD_H)
	if (HAVE_DELTACLOUD_H)
	  find_multiple( "deltacloud" LIBDELTACLOUD_FOUND )
	endif()
endif()

if (LIBDELTACLOUD_FOUND)
	message (STATUS "external configured (LIBDELTACLOUD_FOUND=${LIBDELTACLOUD_FOUND})")
	set( LIBDELTACLOUD_FOUND ${LIBDELTACLOUD_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_LIBDELTACLOUD ON PARENT_SCOPE )
else()
	message (STATUS "external skipped (libdeltacloud)")
endif(LIBDELTACLOUD_FOUND)
