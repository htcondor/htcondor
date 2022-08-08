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
if (WINDOWS)
	condor_pre_external( BOOST boost-1.66.0 "lib;boost" "done")
	if (BOOST_DOWNLOAD_WIN)
		set (BOOST_CONFIGURE echo "Nothing to configure")
		set (BOOST_MAKE echo "No make necessary")
		set (BOOST_INSTALL tar -pczf boost.tar.gz boost/ && tar -zxvf boost.tar.gz -C "${BOOST_INSTALL_LOC}/boost/" && cp *.lib ${BOOST_INSTALL_LOC}/lib && cp *.dll ${BOOST_INSTALL_LOC}/lib && touch ${BOOST_INSTALL_LOC}/done)
 
		ExternalProject_Add(boost
			#-- Download Step ----------
			DOWNLOAD_DIR ${BOOST_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/${BOOST_DOWNLOAD_WIN}
			CONFIGURE_COMMAND ${BOOST_CONFIGURE}
			#--install Step ----------
			BUILD_COMMAND ${BOOST_MAKE}
			BUILD_IN_SOURCE 1
			INSTALL_DIR ${BOOST_INSTALL_LOC}
			INSTALL_COMMAND ${BOOST_INSTALL})

		set(BOOST_ROOT ${BOOST_INSTALL_LOC} PARENT_SCOPE)
		set(BOOST_SHORTVER 1.66 PARENT_SCOPE)
		condor_post_external( boost "boost" "lib" "" )
	endif(BOOST_DOWNLOAD_WIN)

else (WINDOWS)

	set(Boost_USE_MULTITHREADED ON)

	if (NOT PROPER) 
		set (BOOST_COMPONENTS thread) 
	else()

		if (BUILD_TESTING) 
			set (BOOST_COMPONENTS unit_test_framework ${BOOST_COMPONENTS})
		endif()
		if (WITH_PYTHON_BINDINGS)
			set (BOOST_COMPONENTS python ${BOOST_COMPONENTS})
		endif()

	endif()

	# The following is helpful if you are trying to debug cmake find module
    # set (Boost_DEBUG TRUE)

    message (STATUS "SYSTEM_NAME: ${SYSTEM_NAME}" )
    if (DEFINED SYSTEM_NAME)
        if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "64")
            if ( ${SYSTEM_NAME} MATCHES "rhel7" OR ${SYSTEM_NAME} MATCHES "centos7" OR ${SYSTEM_NAME} MATCHES "sl7" OR ${SYSTEM_NAME} MATCHES "amzn2")
                # If you look for python on cmake 2.8.12.2 it fails.
                # Looking for thread finds boost for python for me.
                set (BOOST_COMPONENTS thread)
                if (NOT WANT_PYTHON_WHEELS)
                    message (STATUS "Using system boost169 for 64-bit EL7" )
                    set(BOOST_INCLUDEDIR "/usr/include/boost169")
                    set(BOOST_LIBRARYDIR "/usr/lib64/boost169")
                    set(Boost_INCLUDE_DIR "/usr/include/boost169")
                    set(Boost_LIBRARY_DIR "/usr/lib64/boost169")
                endif()
                set(USE_SYSTEM_BOOST True)
            endif()
        endif()
        if (${SYSTEM_NAME} MATCHES "Debian" OR ${SYSTEM_NAME} MATCHES "Ubuntu" OR ${SYSTEM_NAME} MATCHES "rhel8" OR ${SYSTEM_NAME} MATCHES "centos8" OR ${SYSTEM_NAME} MATCHES "fc" OR ${SYSTEM_NAME} MATCHES "almalinux8" OR ${SYSTEM_NAME} MATCHES "rocky8")
            set(USE_SYSTEM_BOOST True)
            # BOOST_COMPONENTS breaks CMake boost detection on Ubuntu 20.04, Debian bullseye
            set(BOOST_COMPONENTS "" )
        endif()
    endif()

	message (STATUS "Boost components: ${BOOST_COMPONENTS}" )
	find_package( Boost 1.33.1 COMPONENTS ${BOOST_COMPONENTS} )

	# always prefer system libraries where possible
	if(Boost_FOUND)

		if (NOT PREFER_CPP11)
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
			set (BOOST_LD ${Boost_LIBRARY_DIRS})
            if (PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND)
                set(Boost_PYTHON_LIBRARY "external" PARENT_SCOPE)
            endif()
		endif()

	endif(Boost_FOUND)

	# we only use our version of boost if the system is not good enough
    if (NOT PROPER AND NOT USE_SYSTEM_BOOST)

		if (WINDOWS)
			# building boost on windows here doesn't work right now. must use the pre-built boost external.
			#set(TOOLSET msvc-${MSVCVERNUM})
			#set(BOOTSTRAP call bootstrap.bat )
			#set(EXEC .\\)
			#set(BUILD_OPTIONS )
			#set(INCLUDE_LOC include/boost_1.66)
			message (FATAL_ERROR "WINDOWS builds must use pre-built boost")
		else()
			# Boost's detection of the python include directory
			# assumes the directory name doesn't have a
			# suffix (e.g. /usr/include/python-3.7m).
			# But this is common in the linux distros.
			# This only appears to affect Python 3.
			# NOTE: This will fail if PYTHON_INCLUDE_DIRS
			#   has multiple directories.
			if("${PYTHONLIBS_VERSION_STRING}" VERSION_LESS "3.0.0")
				set(BUILD_OPTIONS --layout=system variant=release cxxflags=-fPIC linkflags=-fPIC)
			else()
				file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/python-config.jam
					"using python : : ${PYTHON_EXECUTABLE} : ${PYTHON_INCLUDE_DIRS} ;\n")
				set(BUILD_OPTIONS --user-config=${CMAKE_CURRENT_BINARY_DIR}/python-config.jam --layout=system variant=release cxxflags=-fPIC linkflags=-fPIC)
			endif()

			if (APPLE)
				if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
					set(TOOLSET clang-darwin)
				else()
					set(TOOLSET darwin)
				endif()
				if (cxx_11)
					set(BUILD_OPTIONS ${BUILD_OPTIONS} "cxxflags=-std=c++11 -stdlib=libc++ -fPIC" "linkflags=-std=c++11 -stdlib=libc++ -fPIC")
				endif()
			else()
				set(TOOLSET gcc)
			endif()

			set(INCLUDE_LOC include)
			set(BOOTSTRAP ./bootstrap.sh )
			set(EXEC ./)
		endif()

		set(BOOST_FILENAME boost_1_66_0)
		condor_pre_external( BOOST boost-1.66.0-p1 "lib;boost" "done")

		set(BOOST_MIN_BUILD_DEP --with-thread --with-test --with-python)
		if (LINUX)
			set(BOOST_MIN_BUILD_DEP ${BOOST_MIN_BUILD_DEP} --with-filesystem --with-regex --with-program_options --with-date_time)
		endif()
		set(BOOST_INSTALL echo "nothing")
		unset(BOOST_INCLUDE)

		if (PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND)
			message(STATUS "Bootstrapping python for boost")
			set(Boost_PYTHON_LIBRARY "external" PARENT_SCOPE)
			set(BOOTSTRAP_PYTHON_OPTION "--with-python=${PYTHON_EXECUTABLE}")
		else ()
			set(BOOTSTRAP_PYTHON_OPTION )
		endif()

		# so the idea of privately staging boost is silly.
		ExternalProject_Add(boost
			#-- Download Step ----------
			 DOWNLOAD_DIR ${BOOST_STAGE}/dl
			 URL ${EXTERNALS_SOURCE_URL}/${BOOST_FILENAME}.tar.gz
			#--Patch step ----------
			PATCH_COMMAND patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/no_numpy.patch -p0 &&
				patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/python37.patch -p0
			#--Configure step ----------
			 CONFIGURE_COMMAND ${BOOTSTRAP} --prefix=${BOOST_INSTALL_LOC} ${BOOTSTRAP_PYTHON_OPTION} ${CMD_TERM} echo "Configure complete"
			#--Build Step ----------
			BUILD_COMMAND ${EXEC}bjam ${BOOST_MIN_BUILD_DEP} --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib define=BOOST_HAS_THREADS ${BUILD_OPTIONS} toolset=${TOOLSET} link=static install
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${BOOST_INSTALL_LOC}
			INSTALL_COMMAND touch ${BOOST_INSTALL_LOC}/done )
				  	
		if (WINDOWS)
			#set(BOOST_ROOT ${BOOST_INSTALL_LOC} PARENT_SCOPE)
			#set(BOOST_SHORTVER 1.66 PARENT_SCOPE)
			#condor_post_external( boost "include/boost-1.66" "lib" )
		else()
			condor_post_external( boost "include" "lib" "${BOOST_INSTALL_LOC}/lib/libboost_thread.a;${BOOST_INSTALL_LOC}/lib/libboost_filesystem.a;${BOOST_INSTALL_LOC}/lib/libboost_system.a" )
		endif()
	endif()
endif(WINDOWS)


# update configure information
if (BOOST_VER)
	message (STATUS "external configured (BOOST_INCLUDE=${BOOST_INCLUDE}) version:(${BOOST_VER}) link directories (${BOOST_LD})")
	set( HAVE_EXT_BOOST ON PARENT_SCOPE )
	set( BOOST_VER ${BOOST_VER} PARENT_SCOPE )
	set( BOOST_INCLUDE ${BOOST_INCLUDE} PARENT_SCOPE )
	set( BOOST_LD ${BOOST_LD} PARENT_SCOPE )
	
	if (Boost_PYTHON_LIBRARY)
        set( Boost_PYTHON_LIBRARY ${Boost_PYTHON_LIBRARY} PARENT_SCOPE )
        dprint("Found Python Library: ${Boost_PYTHON_LIBRARY} ")
    endif()
	
  # Set the target dependencies which the rest of condor depends on.
  if (WINDOWS)
	# note that on windows we ship a different library for debug vs. release builds (not that we actually ship debug...)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_python-vc141-mt-1.66.dll
		DESTINATION ${C_LIB}
		CONFIGURATIONS RelWithDebInfo)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_python-vc141-mt-gd-1.66.dll
		DESTINATION ${C_LIB}
		CONFIGURATIONS Debug)
  endif()
	
else(BOOST_VER)
	message (WARNING "**boost not found **")
endif(BOOST_VER)
