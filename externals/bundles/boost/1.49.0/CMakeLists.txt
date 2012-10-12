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

# Search for system's boost libraries.
if (NOT WINDOWS)

    set(Boost_USE_MULTITHREADED ON)

    if (NOT PROPER) 
        set (BOOST_COMPONENTS thread) 
    else()

	if (BUILD_TESTING) 
	  set (BOOST_COMPONENTS unit_test_framework ${BOOST_COMPONENTS})
	endif()

    endif()

    # The following is helpful if you are trying to debug cmake find module
    #  set (Boost_DEBUG TRUE)
    find_package( Boost 1.33.1 COMPONENTS ${BOOST_COMPONENTS} )

    # always prefer system libraries where possible
    if(Boost_FOUND)

	if (NOT PREFER_CPP11 AND NOT PREFER_TR1)
	  check_cxx_source_compiles("
	  #include <boost/unordered_map.hpp>
	  int main() {
	      boost::unordered_map<int,int> foo;
	      return 0;
	  }
	  " HAVE_BOOST_UNORDER_MAP )
      
	  # we don't have any unordered map?
	  if (NOT HAVE_BOOST_UNORDER_MAP)
	    message (STATUS "WARNING did not detect unordered_map")
	    set ( SYSTEM_NOT_UP_TO_SNUFF TRUE )
	  endif()

	endif()

        # set vars b/c we are good to go.
        if (NOT SYSTEM_NOT_UP_TO_SNUFF)
            append_var (CONDOR_EXTERNAL_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
            set (BOOST_VER ${Boost_VERSION})
            set (BOOST_INCLUDE ${Boost_INCLUDE_DIRS})
        endif()

    endif(Boost_FOUND)

endif( NOT WINDOWS )


# we only use our version of boost if the system is not good enough
if (NOT PROPER) # AND (NOT Boost_FOUND OR SYSTEM_NOT_UP_TO_SNUFF) )

	if (WINDOWS)
	  # need to expand the configure and
	  set(TOOLSET msvc-${MSVCVERNUM})
	  set(BOOTSTRAP call bootstrap.bat )
	  set(EXEC .\\)

	  set(BUILD_OPTIONS )
	  set(INCLUDE_LOC include/boost_1_49)
	else()

		set(BUILD_OPTIONS --layout=system variant=release)

	if (DARWIN)
		if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
			set(TOOLSET clang-darwin)
		else()
			set(TOOLSET darwin)
		endif()
		if (cxx_11)
			set(BUILD_OPTIONS ${BUILD_OPTIONS} "cxxflags=-std=c++11 -stdlib=libc++" "linkflags=-std=c++11 -stdlib=libc++")
		endif()
	else()
		set(TOOLSET gcc)
	endif()

		set(INCLUDE_LOC include)
        set(BOOTSTRAP ./bootstrap.sh )
        set(EXEC ./)
	endif()

	set(BOOST_FILENAME boost_1_49_0)
	condor_pre_external( BOOST ${BOOST_FILENAME}-p2 "lib;${INCLUDE_LOC}" "done")

	set(BOOST_MIN_BUILD_DEP --with-thread --with-test)
	set(BOOST_PATCH echo "nothing")
	set(BOOST_INSTALL echo "nothing")
	unset(BOOST_INCLUDE)

	# so the idea of privately staging boost is silly.
	ExternalProject_Add(boost
		#-- Download Step ----------
		 DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${BOOST_FILENAME}.tar.gz ${CMD_TERM}
		 DOWNLOAD_DIR ${BOOST_STAGE}/dl
		 URL http://parrot.cs.wisc.edu/externals/${BOOST_FILENAME}.tar.gz
		#--Patch step ----------
		 PATCH_COMMAND ${BOOST_PATCH}
		#--Configure step ----------
		 CONFIGURE_COMMAND cd ${BOOST_FILENAME} ${CMD_TERM} ${BOOTSTRAP} --prefix=${BOOST_INSTALL_LOC} ${CMD_TERM} echo "Configure complete"
		#--Build Step ----------
		BUILD_COMMAND cd ${BOOST_FILENAME} ${CMD_TERM} ${EXEC}bjam ${BOOST_MIN_BUILD_DEP} --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib define=BOOST_HAS_THREADS ${BUILD_OPTIONS} toolset=${TOOLSET} link=static install
		BUILD_IN_SOURCE 1
		#--install Step ----------
		INSTALL_DIR ${BOOST_INSTALL_LOC}
		INSTALL_COMMAND touch ${BOOST_INSTALL_LOC}/done )
				  	
	if (WINDOWS)
		set(BOOST_ROOT ${BOOST_INSTALL_LOC} PARENT_SCOPE)
		set(BOOST_SHORTVER 1_49 PARENT_SCOPE)
		condor_post_external( boost "include/boost-1_49" "lib" )
	else()
		condor_post_external( boost "include" "lib" )
	endif()	

endif()

# update configure information
if (BOOST_VER)
	message (STATUS "external configured (BOOST_INCLUDE=${BOOST_INCLUDE}) version:(${BOOST_VER})")
	set( HAVE_EXT_BOOST ON PARENT_SCOPE )
	set( BOOST_VER ${BOOST_VER} PARENT_SCOPE )
	set( BOOST_INCLUDE ${BOOST_INCLUDE} PARENT_SCOPE )
else(BOOST_VER)
	message (WARNING "**boost not found **, unit tests will NOT be built.")
endif(BOOST_VER)
