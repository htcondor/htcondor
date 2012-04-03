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

# The source tarball for this external was downloaded from this URL:
#   http://jra1mw.cvs.cern.ch:8180/cgi-bin/jra1mw.cgi/org.glite.ce.blahp.tar.gz?view=tar&pathrev=glite-ce-blahp_R_1_16_5_1

if ( LINUX )
	option(WITH_CAMPUSFACTORY "Compiling with support for Campus Factory" OFF)
endif ( LINUX )

if ( WITH_CAMPUSFACTORY )

	if ( NOT PROPER )
        add_dependencies( campus_factory ${BLAHP_FOUND} )

		### set the install
		install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/python-lib DESTINATION ${C_LIBEXEC}/campus_factory USE_SOURCE_PERMISSIONS )
        install ( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/share DESTINATION ${C_LIBEXEC}/campus_factory USE_SOURCE_PERMISSIONS )
        install ( FILES ${CMAKE_CURRENT_SOURCE_DIR}/bin/cf DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS}) 
		
		set(CAMPUSFACTORY_FOUND ON)

	else ( NOT PROPER )

		
	endif ( NOT PROPER )


endif( WITH_CAMPUSFACTORY )

if ( CAMPUSFACTORY_FOUND )
	message( STATUS "external configured (campusfactory) CAMPUSFACTORY_FOUND=${CAMPUSFACTORY_FOUND}" )
	set( CAMPUSFACTORY_FOUND ${CAMPUSFACTORY_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_CAMPUSFACTORY ON PARENT_SCOPE )
else()
	message ( STATUS "external skipped (campusfactory)" )
endif ( CAMPUSFACTORY_FOUND )
	
