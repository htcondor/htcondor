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

# NOTE: libconfuse is needed to build ganglia, but the part of ganglia we
# actually care about (libganglia) does not depend on it.

if (LINUX)
  option(WITH_CONFUSE "Compiling with support for CONFUSE (needed by ganglia)" ON)
endif(LINUX)

if (WITH_CONFUSE)

    find_multiple( "confuse" CONFUSE_FOUND )
    check_include_files("confuse.h" HAVE_CONFUSE_H)

    if ( NOT PROPER AND NOT (CONFUSE_FOUND AND HAVE_CONFUSE_H) )

        condor_pre_external( CONFUSE confuse-2.7 "lib;include" "include/confuse.h")

        if (${SYS_ARCH} MATCHES "64" )
            # ganglia expects to find libconfuse in lib64, but
	    # libconfuse by default puts it in lib.
            SET(CONFUSE_LIBDIR lib64)
        else()
            SET(CONFUSE_LIBDIR lib)
        endif()

        ExternalProject_Add(confuse
                    DEPENDS ""
                    #-- Download Step ----------
                    DOWNLOAD_DIR ${CONFUSE_STAGE}/dl
                    URL http://parrot.cs.wisc.edu/externals/confuse-2.7.tar.gz
                    #--Patch Step ----------
                    PATCH_COMMAND ""
                    #--Configure step ----------
                    CONFIGURE_COMMAND ./configure --prefix=${CONFUSE_INSTALL_LOC} --libdir=${CONFUSE_INSTALL_LOC}/${CONFUSE_LIBDIR} --with-pic
                    #--Build Step ----------
                    BUILD_COMMAND make
                    BUILD_IN_SOURCE 1
                    #--install Step ----------
                    INSTALL_DIR ${CONFUSE_INSTALL_LOC}
                    INSTALL_COMMAND make install )

        set(BUILDING_CONFUSE ON PARENT_SCOPE)
        set(CONFUSE_FOUND "${CONFUSE_INSTALL_LOC}/${CONFUSE_LIBDIR}/libconfuse.a")
        condor_post_external( confuse include lib )
    endif()

endif(WITH_CONFUSE)

if (CONFUSE_FOUND)
    message (STATUS "external configured (CONFUSE_FOUND=${CONFUSE_FOUND})")
    # Set the target dependencies which the rest of condor depends on.
    set(CONFUSE_FOUND "${CONFUSE_FOUND}" PARENT_SCOPE)
    set( HAVE_EXT_CONFUSE ON PARENT_SCOPE )
else(CONFUSE_FOUND)
    message (STATUS "external skipped (confuse)")
endif(CONFUSE_FOUND)
