 ###############################################################
 #
 # Copyright (C) 1990-2010, Redhat.
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

option(WITH_LIBCGROUP "Compiling with support for libcgroup" ON)

if (WITH_LIBCGROUP)

	if ( NOT PROPER )

		set (LIBCGROUP_VER libcgroup-0.37)

		set (LIBCGROUP_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE} --disable-dependency-tracking CPPFLAGS=-I${EXTERNAL_STAGE}/include/)
		set (LIBCGROUP_MAKE make )
		set (LIBCGROUP_INSTALL make install )

		ExternalProject_Add( libcgroup
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${LIBCGROUP_VER}
				    #-- Download Step ----------
				    DOWNLOAD_COMMAND wget -N http://cdnetworks-us-1.dl.sourceforge.net/project/libcg/libcgroup/v0.37/${LIBCGROUP_VER}.tar.bz2 ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://cdnetworks-us-1.dl.sourceforge.net/project/libcg/libcgroup/v0.37/${LIBCGROUP_VER}.tar.bz2
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd ${LIBCGROUP_VER} ${CMD_TERM} ${LIBCGROUP_CONFIGURE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${LIBCGROUP_VER} ${CMD_TERM} ${LIBCGROUP_MAKE}
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd ${LIBCGROUP_VER} ${CMD_TERM} ${LIBCGROUP_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		set(LIBCGROUP_FOUND "${EXTERNAL_STAGE}/lib/libcgroup.a")

		set ( LIBCGROUP_SHARE "${EXTERNAL_BUILD_PREFIX}/${LIBCGROUP_VER}/src/libcgroup/${LIBCGROUP_VER}/libcgroup" )
		append_var (CONDOR_EXTERNALS libcgroup)

	else( NOT PROPER )

		find_multiple( "cgroup" LIBCGROUP_FOUND)

	endif( NOT PROPER )

	if (LIBCGROUP_FOUND)

		set( LIBCGROUP_FOUND ${LIBCGROUP_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_LIBCGROUP ON PARENT_SCOPE )
		message ( STATUS "external configured (LIBCGROUP_FOUND=${LIBCGROUP_FOUND})" )

	endif (LIBCGROUP_FOUND)

else(WITH_LIBCGROUP)

	message (STATUS "external skipped (libcgroup)")

endif(WITH_LIBCGROUP)
