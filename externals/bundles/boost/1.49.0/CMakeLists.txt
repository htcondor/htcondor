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
#option( WITH_BOOST "Compiling with support for BOOST" ON )

# could be modified.
#if (WITH_BOOST)
  
  if (NOT PROPER)

		condor_pre_external( BOOST boost_1_49_0 "lib;include" "include/boost")
		
		if (WINDOWS)
		  # need to expand the configure and
		  set(TOOLSET msvc)
		  set(JAM_EXT bat)
		  set(EXEC .\\)
		else()

		  if (DARWIN)
		  	set(TOOLSET darwin)
		  else()
		  	set(TOOLSET gcc)
		  endif()

		  #set(TOOLSET gcc)
		  set(JAM_EXT sh)
		  set(EXEC ./)
		endif()

		#if(LINUX)
			set(BOOST_MIN_BUILD_DEP --with-thread)
		#else()
		#	set(BOOST_MIN_BUILD_DEP --without-date_time --without-filesystem --without-graph --without-iostreams --without-math --without-mpi --without-program_options --without-python --without-regex --without-serialization --without-signals --without-system --without-test --without-thread --without-wave)
		#endif()

		set(BOOST_PATCH echo "nothing")

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
			set(BOOST_INSTALL echo "nothing")
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
				    ${EXEC}bjam ${BOOST_MIN_BUILD_DEP} --layout=system --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib define=BOOST_HAS_THREADS link=static variant=release toolset=${TOOLSET} install
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

	condor_post_external( boost "include" "lib" )
	
  else()

	# Search for system's boost libraries.
	set(Boost_USE_MULTITHREADED ON)
	find_package( Boost 1.33.1 COMPONENTS thread program_options system regex filesystem)

	# For proper builds use the .so/.dll's .
	if(Boost_FOUND)
		append_var (CONDOR_EXTERNAL_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
		set (BOOST_VER ${Boost_VERSION})
		set (BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
	endif(Boost_FOUND)

  endif()

#endif()

# update configure information
if (BOOST_VER)
	message (STATUS "external configured (BOOST_INCLUDE=${BOOST_INCLUDE}) version:(${BOOST_VER})")
	set( HAVE_EXT_BOOST ON PARENT_SCOPE )
	set( BOOST_VER ${BOOST_VER} PARENT_SCOPE )
	set( BOOST_INCLUDE ${BOOST_INCLUDE} PARENT_SCOPE )
else(BOOST_VER)
	message (FATAL_ERROR "boost not found, and required for build")
endif(BOOST_VER)
