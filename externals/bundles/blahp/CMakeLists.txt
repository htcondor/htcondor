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

# We now get blahp from an rpm - it's not used for the build but the
# blahp location still needs to be configured

if ( LINUX )
	option(WITH_BLAHP "Compiling with support for BLAHP" ON)
endif ( LINUX )

if ( WITH_BLAHP )
	# provide default blahp location from rpm if not explicitly configured
	if ( NOT BLAHP_FOUND )
		set( BLAHP_FOUND /usr/libexec/blahp/BLClient )
	endif ( NOT BLAHP_FOUND )
endif( WITH_BLAHP )

if ( BLAHP_FOUND )
	message( STATUS "external configured (blahp) BLAHP_FOUND=${BLAHP_FOUND}" )
	set( BLAHP_FOUND ${BLAHP_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_BLAHP ON PARENT_SCOPE )
else()
	message ( STATUS "external skipped (blahp)" )
endif ( BLAHP_FOUND )
	
