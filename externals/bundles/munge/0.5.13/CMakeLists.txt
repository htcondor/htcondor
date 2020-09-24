 ###############################################################
 #
 # Copyright 2018, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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

if (LINUX)
	option(WITH_MUNGE "Compiling with support for MUNGE" ON)
endif(LINUX)

if (WITH_MUNGE)

	if ( NOT PROPER )

		condor_pre_external( MUNGE munge-0.5.13 "lib;include" "done" )

		ExternalProject_Add(munge
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${MUNGE_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/munge-0.5.13.tar.gz
					#--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${MUNGE_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${MUNGE_INSTALL_LOC}
				    INSTALL_COMMAND make install && touch ${MUNGE_INSTALL_LOC}/done)

		# Set the target dependencies which the rest of condor depends on.
		set( LIBMUNGE_SO "libmunge.so.2" )
		set (MUNGE_FOUND "${MUNGE_INSTALL_LOC}/lib/libmunge.so")

		condor_post_external( munge include OFF "${MUNGE_INSTALL_LOC}/lib/libmunge.so" )

	else(NOT PROPER)

		find_library( MUNGE_FOUND "munge" )
		find_so_name( LIBMUNGE_SO ${MUNGE_FOUND} )

	endif(NOT PROPER)

	if( DLOPEN_SECURITY_LIBS )
		set( MUNGE_FOUND "" CACHE STRING "Munge Found" FORCE )
	endif()

	message( STATUS "external configured (MUNGE_FOUND=${MUNGE_FOUND})" )
	set( HAVE_EXT_MUNGE ON PARENT_SCOPE )
	set( MUNGE_FOUND ${MUNGE_FOUND} PARENT_SCOPE )
	set( LIBMUNGE_SO ${LIBMUNGE_SO} PARENT_SCOPE )

else(WITH_MUNGE)

	message (STATUS "external skipped (munge)")

endif(WITH_MUNGE)
