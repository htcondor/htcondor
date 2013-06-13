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

if (LINUX)
  option(WITH_APR "Compiling with support for APR (needed by ganglia)" ON)
endif(LINUX)

if (WITH_APR)

    find_multiple( "apr" APR_FOUND )
    check_include_files("apr-1/apr.h" HAVE_APR_H)

    if ( NOT PROPER AND NOT (APR_FOUND AND HAVE_APR_H) )

        condor_pre_external( APR apr-1.4.6 "lib;include" "include/apr-1/apr.h")

        ExternalProject_Add(apr
                    DEPENDS ""
                    #-- Download Step ----------
                    DOWNLOAD_DIR ${APR_STAGE}/dl
                    URL http://parrot.cs.wisc.edu/externals/apr-1.4.6.tar.gz
                    #--Patch Step ----------
                    PATCH_COMMAND ""
                    #--Configure step ----------
                    CONFIGURE_COMMAND ./configure --prefix=${APR_INSTALL_LOC}
                    #--Build Step ----------
                    BUILD_COMMAND make
                    BUILD_IN_SOURCE 1
                    #--install Step ----------
                    INSTALL_DIR ${APR_INSTALL_LOC}
                    INSTALL_COMMAND make install )

        set(BUILDING_APR ON PARENT_SCOPE)
        set(APR_FOUND "${APR_INSTALL_LOC}/lib/libapr-1.so")
        condor_post_external( apr include lib )

	# Install the APR libraries manually.
	# In order to use an install(FILES) command, we'd
	# have to explicitly list evey file.
	# We can't use a file(GLOB) because that's evaluated
	# when cmake is run, which may be before the
	# external is built.
	install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
	install( CODE "execute_process(COMMAND find ${APR_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
		COMMAND xargs -isrc cp -a src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
	if ( CONDOR_STRIP_PACKAGES )
		# Strip the libraries manually.
		# Since cmake didn't create the libraries
		# itself, it doesn't know to strip them.
		install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libapr*.so*
			COMMAND xargs strip)" )
	endif()

    endif()

endif(WITH_APR)

if (APR_FOUND)
    message (STATUS "external configured (APR_FOUND=${APR_FOUND})")
    # Set the target dependencies which the rest of condor depends on.
    set(APR_FOUND "${APR_FOUND}" PARENT_SCOPE)
    set( HAVE_EXT_APR ON PARENT_SCOPE )
else(APR_FOUND)
    message (STATUS "external skipped (apr)")
endif(APR_FOUND)
