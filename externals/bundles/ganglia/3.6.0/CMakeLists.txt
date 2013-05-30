 ###############################################################
 #
 # Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
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

if (WITH_GANGLIA)

    if ( PROPER )
        if( NOT GANGLIA_FOUND )
            message(FATAL_ERROR "Missing libganglia.")
        elif( NOT HAVE_GANGLIA_H )
            message(FATAL_ERROR "Missing ganglia.h.")
        endif()
    else()

        if( BUILDING_APR )
            set( APR_FLAG --with-libapr=${APR_INSTALL_LOC}/bin/apr-1-config )
        else()
            set( APR_FLAG "" )
        endif( BUILDING_APR )

        if( BUILDING_CONFUSE )
            set( CONFUSE_FLAG --with-libconfuse=${CONFUSE_INSTALL_LOC} )
        else()
            set( CONFUSE_FLAG "" )
        endif( BUILDING_CONFUSE )

        if (${SYS_ARCH} MATCHES "64" )
            SET(GANGLIA_LIBDIR lib64)
        else()
            SET(GANGLIA_LIBDIR lib)
            # inclusion of apr.h fails on 32-bit systems without the following
            SET(GANGLIA_CFLAGS CFLAGS=-D_LARGEFILE64_SOURCE)
        endif()

        condor_pre_external( GANGLIA ganglia-3.6.0 "${GANGLIA_LIBDIR};include" "include/ganglia.h")

	# NOTE: for ganglia-3.6.0, I did a slightly sneaky thing: I copied gmond/gmond.service.in
	# from the ganglia github repository into the official tarball, because a bug in their
	# packaging omitted this file.  This fixes builds on Fedora 17 and 18.

        ExternalProject_Add(ganglia
                    DEPENDS ${APR_REF} ${CONFUSE_REF}
                    #-- Download Step ----------
                    DOWNLOAD_DIR ${GANGLIA_STAGE}/dl
                    URL http://parrot.cs.wisc.edu/externals/ganglia-3.6.0.tar.gz
                    #--Patch Step ----------
                    PATCH_COMMAND ""
                    #--Configure step ----------
                    CONFIGURE_COMMAND ./configure --prefix=${GANGLIA_INSTALL_LOC} ${APR_FLAG} ${CONFUSE_FLAG} ${GANGLIA_CFLAGS}
                    #--Build Step ----------
                    BUILD_COMMAND make
                    BUILD_IN_SOURCE 1
                    #--install Step ----------
                    INSTALL_DIR ${GANGLIA_INSTALL_LOC}
                    INSTALL_COMMAND make install )

        set(GANGLIA_FOUND "${GANGLIA_INSTALL_LOC}/${GANGLIA_LIBDIR}/libganglia.so")
        condor_post_external( ganglia include ${GANGLIA_LIBDIR} )

	# Install the GANGLIA libraries manually.
	# In order to use an install(FILES) command, we'd
	# have to explicitly list evey file.
	# We can't use a file(GLOB) because that's evaluated
	# when cmake is run, which may be before the
	# external is built.
	install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
	install( CODE "execute_process(COMMAND find ${GANGLIA_INSTALL_LOC}/${GANGLIA_LIBDIR} -maxdepth 1 -name *.so*
		COMMAND xargs -isrc cp -a src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
	if ( CONDOR_STRIP_PACKAGES )
		# Strip the libraries manually.
		# Since cmake didn't create the libraries
		# itself, it doesn't know to strip them.
		install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libganglia*.so*
			COMMAND xargs strip)" )
	endif()

    endif()

    message (STATUS "external configured (GANGLIA_FOUND=${GANGLIA_FOUND})")
    # Set the target dependencies which the rest of condor depends on.
    set(GANGLIA_FOUND "${GANGLIA_FOUND}" PARENT_SCOPE)
    set( HAVE_EXT_GANGLIA ON PARENT_SCOPE )

else()
    message (STATUS "external skipped (ganglia)")
endif(WITH_GANGLIA)
