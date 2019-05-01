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
  option(WITH_BOINC "Compiling with support for submission to BOINC" ON)
endif()

if ( WITH_BOINC )

	if ( NOT PROPER )

		condor_pre_external( BOINC boinc-client_release-7.14-7.14.1 "lib;include" "done")

		set( BOINC_PATCH patch -p1 -N -i ${CMAKE_CURRENT_SOURCE_DIR}/boinc.patch )

		ExternalProject_Add(boinc
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${BOINC_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/${BOINC_VER}.tar.gz
				    #--Patch step ----------
				    PATCH_COMMAND ${BOINC_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./_autosetup && ./configure --disable-server --disable-client --disable-manager
				    #--Build Step ----------
				    BUILD_COMMAND make svn_version.h && cd lib && make && cd ../samples/condor && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${BOINC_INSTALL_LOC}
				    INSTALL_COMMAND cp samples/condor/boinc_gahp ${BOINC_INSTALL_LOC} && touch ${BOINC_INSTALL_LOC}/done)

		# Set the target dependencies which the rest of condor depends on.
		set( BOINC_FOUND ON )

		### set the install
		install ( PROGRAMS "${BOINC_INSTALL_LOC}/boinc_gahp" DESTINATION ${C_SBIN} )

		condor_post_external( boinc OFF OFF "" )

	else( NOT PROPER )

		find_program( BOINC_FOUND boinc_gahp HINTS /usr/bin )
		dprint("Scanning for boinc_gahp...(${BOINC_FOUND})")

	endif( NOT PROPER )

	if ( BOINC_FOUND )
		message (STATUS "external configured (BOINC_FOUND=${BOINC_FOUND})")
		set( BOINC_FOUND ${BOINC_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_BOINC ON PARENT_SCOPE )
	endif(BOINC_FOUND)

else(WITH_BOINC)

	message (STATUS "external skipped (boinc)")

endif(WITH_BOINC)
