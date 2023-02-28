 ###############################################################
 #
 # Copyright (C) 2022, Condor Team, Computer Sciences Department,
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

# boost is only used for python bindings
if(WITH_PYTHON_BINDINGS)

# Search for system's boost libraries.
if (WINDOWS)
	condor_pre_external( BOOST boost-1.78.0 "lib;boost" "done")
	if(MSVC_VERSION LESS 1930)
		# this one has python 3.6, 3.8, 3.9 adn 3.10
		set(BOOST_DOWNLOAD_WIN boost-1.78.0-VC140.tar.gz)
		set(BOOST_DOWNLOAD_MD5SUM 322b677c458819d3f08dcdabbdd3d108)
		set(BOOST_DLL_VCVER vc140)
	else ()
		# this one includes python 3.8, 3.9 and 3.10
		set(BOOST_DOWNLOAD_WIN boost-1.78.0-VC143.full.tar.gz)
		set(BOOST_DOWNLOAD_MD5SUM 5bae48bc08d355cca509ecab05e320d1)
		set(BOOST_DLL_VCVER vc143)
	endif ()

	set(BOOST_CONFIGURE echo "Nothing to configure")
	set(BOOST_MAKE echo "No make necessary")
	set(BOOST_INSTALL tar -czpf boost.tar.gz boost && tar -xzvf boost.tar.gz -C "${BOOST_INSTALL_LOC}/boost/" && ${CMAKE_COMMAND} -E copy_directory . ${BOOST_INSTALL_LOC}/lib && ${CMAKE_COMMAND} -E touch ${BOOST_INSTALL_LOC}/done)

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
	set(BOOST_SHORTVER 1_78 PARENT_SCOPE)
	condor_post_external( boost "boost" "lib" "" )

else (WINDOWS)

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
		${BOOST_INSTALL_LOC}/lib/boost_${PYTHON3_LIB_BASENAME}-${BOOST_DLL_VCVER}-mt-x64-1_78.dll
		DESTINATION ${C_LIB}
		CONFIGURATIONS RelWithDebInfo)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_${PYTHON3_LIB_BASENAME}-${BOOST_DLL_VCVER}-mt-gd-x64-1_78.dll
		${BOOST_INSTALL_LOC}/lib/boost_${PYTHON3_LIB_BASENAME}-${BOOST_DLL_VCVER}-mt-gd-x64-1_78.pdb
		DESTINATION ${C_LIB}
		CONFIGURATIONS Debug)
  endif()

else(BOOST_VER)
	message (WARNING "**boost not found **")
endif(BOOST_VER)

endif(WITH_PYTHON_BINDINGS)
