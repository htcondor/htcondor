 ###############################################################
 #
 # 64Copyright 2011 Red Hat, Inc.
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

# boost is only used for python bindings
if(WITH_PYTHON_BINDINGS)

# Search for system's boost libraries.
if (WINDOWS)
	condor_pre_external( BOOST boost-1.68.0 "lib;boost" "done")
	# a7c65f8d2b1614a0d48fb93ec78e4b7f  boost-1.68.0-VC15.tar.gz
	# 87f74d9e539493bbed004bc10a4f0497  boost-1.68.0-VC141.full.tar.gz
	set(BOOST_DOWNLOAD_WIN boost-1.68.0-VC141.full.tar.gz)
	set(BOOST_DOWNLOAD_MD5SUM 87f74d9e539493bbed004bc10a4f0497)
	set(BOOST_DLL_VCVER vc141)
	set(BOOST_ADDL_REL_DLLS ${BOOST_INSTALL_LOC}/lib/boost_python38-${BOOST_DLL_VCVER}-mt-x64-1_68.dll ${BOOST_INSTALL_LOC}/lib/boost_python39-${BOOST_DLL_VCVER}-mt-x64-1_68.dll)
	set(BOOST_ADDL_DBG_DLLS ${BOOST_INSTALL_LOC}/lib/boost_python38-${BOOST_DLL_VCVER}-mt-gd-x64-1_68.dll ${BOOST_INSTALL_LOC}/lib/boost_python39-${BOOST_DLL_VCVER}-mt-gd-x64-1_68.dll)

	# if (BOOST_DOWNLOAD_WIN)
		set (BOOST_CONFIGURE echo "Nothing to configure")
		set (BOOST_MAKE echo "No make necessary")
		set (BOOST_INSTALL tar -czpf boost.tar.gz boost && tar -xzvf boost.tar.gz -C "${BOOST_INSTALL_LOC}/boost/" && cp *boost_python* ${BOOST_INSTALL_LOC}/lib && touch ${BOOST_INSTALL_LOC}/done)
 
		ExternalProject_Add(boost
			#-- Download Step ----------
			DOWNLOAD_DIR ${BOOST_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/${BOOST_DOWNLOAD_WIN}
			URL_MD5 ${BOOST_DOWNLOAD_MD5SUM}
			CONFIGURE_COMMAND ${BOOST_CONFIGURE}
			#--install Step ----------
			BUILD_COMMAND ${BOOST_MAKE}
			BUILD_IN_SOURCE 1
			INSTALL_DIR ${BOOST_INSTALL_LOC}
			INSTALL_COMMAND ${BOOST_INSTALL})

		set(BOOST_ROOT ${BOOST_INSTALL_LOC} PARENT_SCOPE)
		set(BOOST_SHORTVER 1_68 PARENT_SCOPE)
		condor_post_external( boost "boost" "lib" "" )
	# endif(BOOST_DOWNLOAD_WIN)

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
	#  set (Boost_DEBUG TRUE)
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
		endif()

	endif(Boost_FOUND)

	# we only use our version of boost if the system is not good enough
	if (NOT PROPER)

		if (WINDOWS)
			message (FATAL_ERROR "WINDOWS builds must use pre-built boost")
		else()

			set(BUILD_OPTIONS --layout=system variant=release cxxflags=-fPIC linkflags=-fPIC)

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

		set(BOOST_FILENAME boost_1_68_0)
		condor_pre_external( BOOST boost-1.68.0 "lib;boost" "done")

		set(BOOST_MIN_BUILD_DEP --with-thread --with-test --with-python)
		if (LINUX)
			set(BOOST_MIN_BUILD_DEP ${BOOST_MIN_BUILD_DEP} --with-filesystem --with-regex --with-program_options --with-date_time)
		endif()
		set(BOOST_INSTALL echo "nothing")
		unset(BOOST_INCLUDE)

		if (PYTHONINTERP_FOUND AND PYTHONLIBS_FOUND)
			message(STATUS "Bootstrapping python 2 and 3 for boost")
			set(Boost_PYTHON_LIBRARY "external" PARENT_SCOPE)

			# We should probably change how we write this file
			# depending on which Pythons we actually found.
			file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/python-config.jam
				"using python : ${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR} : ${PYTHON_EXECUTABLE} : ${PYTHON_INCLUDE_DIRS} ;\n"
				"using python : ${PYTHON3_VERSION_MAJOR}.${PYTHON3_VERSION_MINOR} : ${PYTHON3_EXECUTABLE} : ${PYTHON3_INCLUDE_DIRS} ;\n"
			)
			set(BUILD_OPTIONS ${BUILD_OPTIONS} --user-config=${CMAKE_CURRENT_BINARY_DIR}/python-config.jam python=${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR},${PYTHON3_VERSION_MAJOR}.${PYTHON3_VERSION_MINOR})
		else()
			message(STATUS "Bootstrapping python 3 for boost")
			set(Boost_PYTHON_LIBRARY "external" PARENT_SCOPE)

			# We should probably change how we write this file
			# depending on which Pythons we actually found.
			file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/python-config.jam
				"using python : ${PYTHON3_VERSION_MAJOR}.${PYTHON3_VERSION_MINOR} : ${PYTHON3_EXECUTABLE} : ${PYTHON3_INCLUDE_DIRS} ;\n"
			)
			set(BUILD_OPTIONS ${BUILD_OPTIONS} --user-config=${CMAKE_CURRENT_BINARY_DIR}/python-config.jam python=${PYTHON3_VERSION_MAJOR}.${PYTHON3_VERSION_MINOR})
		endif()

		# so the idea of privately staging boost is silly.
		ExternalProject_Add(boost
			#-- Download Step ----------
			 DOWNLOAD_DIR ${BOOST_STAGE}/dl
			 URL ${EXTERNALS_SOURCE_URL}/${BOOST_FILENAME}.tar.gz
			#--Patch step ----------
			 PATCH_COMMAND patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/no_numpy.patch -p0
			#--Configure step ----------
			 CONFIGURE_COMMAND ${BOOTSTRAP} --prefix=${BOOST_INSTALL_LOC} ${CMD_TERM} echo "Configure complete"
			#--Build Step ----------
			BUILD_COMMAND ${EXEC}bjam ${BOOST_MIN_BUILD_DEP} --prefix=${BOOST_INSTALL_LOC} --libdir=${BOOST_INSTALL_LOC}/lib define=BOOST_HAS_THREADS ${BUILD_OPTIONS} toolset=${TOOLSET} link=static install
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${BOOST_INSTALL_LOC}
			INSTALL_COMMAND touch ${BOOST_INSTALL_LOC}/done
		)

		condor_post_external( boost "include" "lib" "${BOOST_INSTALL_LOC}/lib/libboost_thread.a;${BOOST_INSTALL_LOC}/lib/libboost_filesystem.a;${BOOST_INSTALL_LOC}/lib/libboost_system.a" )
	endif(NOT PROPER)
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
  	set(BOOST_DLL_VCVER ${BOOST_DLL_VCVER} PARENT_SCOPE)

	# note that on windows we ship a different library for debug vs. release builds (not that we actually ship debug...)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_python27-${BOOST_DLL_VCVER}-mt-x32-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python36-${BOOST_DLL_VCVER}-mt-x32-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python27-${BOOST_DLL_VCVER}-mt-x64-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python36-${BOOST_DLL_VCVER}-mt-x64-1_68.dll
		${BOOST_ADDL_REL_DLLS}
		DESTINATION ${C_LIB}
		CONFIGURATIONS RelWithDebInfo)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_python27-${BOOST_DLL_VCVER}-mt-gd-x32-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python36-${BOOST_DLL_VCVER}-mt-gd-x32-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python27-${BOOST_DLL_VCVER}-mt-gd-x64-1_68.dll
		${BOOST_INSTALL_LOC}/lib/boost_python36-${BOOST_DLL_VCVER}-mt-gd-x64-1_68.dll
		${BOOST_ADDL_DBG_DLLS}
		DESTINATION ${C_LIB}
		CONFIGURATIONS Debug)
  endif()

else(BOOST_VER)
	message (WARNING "**boost not found **")
endif(BOOST_VER)

endif(WITH_PYTHON_BINDINGS)
