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


# so in condor only cream cares about this
# re-anable DARWIN when bug is fixed.
if (LINUX OR WANT_CONTRIB)
  option( WITH_BOOST "Compiling with support for BOOST" ON )
endif()

# could be modified.
if (WITH_BOOST )

  if (NOT PROPER)

		condor_pre_external( BOOST boost_1_39_0 "lib;include" "include/boost")
		
		if (WINDOWS)
		  # need to expand the configure and
		  set(TOOLSET msvc)
		  set(JAM_EXT bat)
		  set(EXEC .\\)
		else()
		  set(TOOLSET gcc)
		  set(JAM_EXT sh)
		  set(EXEC ./)
		endif()

		if (DARWIN)
		  set(BOOST_PATCH patch -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/macos.patch)
		else()
		  set(BOOST_PATCH echo "nothing")
		endif()

                # so if we want contrib then build the extra elements,
		# otherwise just default to the normal build.
		if (WANT_CONTRIB)
		  set(BOOST_INSTALL ${EXEC}bjam -a --with-thread
		  --with-program_options --with-system --with-regex
		  --with-filesystem --with-date_time --build-type=complete
		  --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib
		  define=BOOST_HAS_THREADS link=shared variant=release toolset=${TOOLSET}
		  install)
		else()
		  set(BOOST_INSTALL echo "Do Nothing")
		endif()


		# so the idea of privately staging boost is silly.
		ExternalProject_Add(boost
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${BOOST_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${BOOST_STAGE}/dl
				    URL http://parrot.cs.wisc.edu/externals/${BOOST_VER}.tar.gz
					#--Patch step ----------
					PATCH_COMMAND ${BOOST_PATCH}
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd ${BOOST_VER} ${CMD_TERM}
				    	./bootstrap.${JAM_EXT} --prefix=${BOOST_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${BOOST_VER} &&
				    ${EXEC}bjam --with-thread --layout=system --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib define=BOOST_HAS_THREADS link=static variant=release toolset=${TOOLSET} install
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${BOOST_INSTALL_LOC}
				    INSTALL_COMMAND cd ${BOOST_VER} && ${BOOST_INSTALL} )
				  	
	
	if (WINDOWS)
		set(BOOST_ROOT ${BOOST_INSTALL_LOC} PARENT_SCOPE)

        if (WANT_CONTRIB)

            # binding dep to VS-2008 :-( there is no easy solution to this
            install ( FILES ${BOOST_INSTALL_LOC}/lib/boost_date_time-vc90-mt-1_39.dll
                        ${BOOST_INSTALL_LOC}/lib/boost_filesystem-vc90-mt-1_39.dll
                        ${BOOST_INSTALL_LOC}/lib/boost_program_options-vc90-mt-1_39.dll
                        ${BOOST_INSTALL_LOC}/lib/boost_regex-vc90-mt-1_39.dll
                        ${BOOST_INSTALL_LOC}/lib/boost_system-vc90-mt-1_39.dll
                        ${BOOST_INSTALL_LOC}/lib/boost_thread-vc90-mt-1_39.dll
                    DESTINATION ${C_LIB} )

        endif()

    else()
        dprint ("TBD: NON-PROPER INSTALL for CONTRIB")
	endif()

	condor_post_external( boost "include" OFF )
	
  else()

	# For proper builds use the .so/.dll's .
	set(Boost_USE_MULTITHREADED ON)
	find_package( Boost 1.33.1 COMPONENTS thread program_options system regex filesystem)
	if(Boost_FOUND)
		append_var (CONDOR_EXTERNAL_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
		set (BOOST_VER ${Boost_VERSION})
	endif(Boost_FOUND)

  endif(NOT PROPER)

endif()

# update configure information
if (BOOST_VER)
	message (STATUS "external configured (BOOST_INCLUDE_FOUND=${BOOST_INCLUDE_FOUND}) version:(${BOOST_VER})")
	set( HAVE_EXT_BOOST ON PARENT_SCOPE )
	set( BOOST_VER ${BOOST_VER} PARENT_SCOPE )
else(BOOST_VER)
	message (STATUS "external skipped (boost)")
endif(BOOST_VER)
