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
condor_pre_external( BOOST boost-1.54.0 "lib;boost" "done")

#set(BOOST_DOWNLOAD ${BOOST_VER}.tar.gz)
if(BOOST_DOWNLOAD_WIN)
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
  set(BOOST_SHORTVER 1_54 PARENT_SCOPE)
  condor_post_external( boost "boost" "lib" "" )
endif()

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
		${BOOST_INSTALL_LOC}/lib/boost_python-vc110-mt-1_54.dll
		DESTINATION ${C_LIB}
		CONFIGURATIONS RelWithDebInfo)
	install ( FILES
		${BOOST_INSTALL_LOC}/lib/boost_python-vc110-mt-gd-1_54.dll
		DESTINATION ${C_LIB}
		CONFIGURATIONS Debug)
  endif()
	
else(BOOST_VER)
	message (WARNING "**boost not found **, unit tests will NOT be built.")
endif(BOOST_VER)
