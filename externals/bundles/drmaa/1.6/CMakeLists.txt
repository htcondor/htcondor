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

if (NOT PROPER AND (LINUX OR WINDOWS))
  option(WITH_DRMAA "Compiling with support for drmaa" ON)
endif()

if ( WITH_DRMAA )

	set ( DRMAA_VER drmaa-1.6)
	set ( DRMAA_STAGE ${EXTERNAL_STAGE}/${DRMAA_VER} )
	set ( DRMAA_INSTALL_LOC ${DRMAA_STAGE}/install )
	file ( MAKE_DIRECTORY ${DRMAA_INSTALL_LOC} ${DRMAA_INSTALL_LOC}/lib ${DRMAA__INSTALL_LOC}/include )

	# search for a local copy to see if one exists.
	if (SCRATCH_EXTERNALS AND EXISTS ${DRMAA_INSTALL_LOC}/include/drmaa.h )
		set (DRMAA_PREBUILT ON)
	endif()

	if (NOT WINDOWS)
		set (DRMAA_CONFIGURE ./configure --prefix=${EXTERNAL_STAGE})
		set (DRMAA_MAKE make)
		set (DRMAA_INSTALL cp libcondordrmaa.a ${DRMAA_INSTALL_LOC}/lib &&
						   cp drmaa.h README ${DRMAA_INSTALL_LOC}/include/ )

		if(DARWIN)
			set (DRMAA_INSTALL cp libdrmaa.dylib ${DRMAA_INSTALL_LOC}/lib/ && ${DRMAA_INSTALL})
			set( DRMAA_FOUND "${DRMAA_INSTALL_LOC}/lib/libdrmaa.dylib;${DRMAA_INSTALL_LOC}/lib/libcondordrmaa.a" )
		else()
			set (DRMAA_INSTALL cp libdrmaa.so ${DRMAA_INSTALL_LOC}/lib/ && ${DRMAA_INSTALL})
			set( DRMAA_FOUND "${DRMAA_INSTALL_LOC}/lib/libdrmaa.so;${DRMAA_INSTALL_LOC}/lib/libcondordrmaa.a" )
		endif()

	else()
		set (DRMAA_CONFIGURE echo "Nothing to configure for Windows")
		set (DRMAA_MAKE nmake /f libdrmaa.mak CFG="libdrmaa - Win32 Release")
		set (DRMAA_INSTALL  cp Release/* ${DRMAA_INSTALL_LOC}/lib \r\n
		                    cp drmaa.h README ${DRMAA_INSTALL_LOC}/include)
			
		set( DRMAA_FOUND "${DRMAA_INSTALL_LOC}/lib/libdrmaa.lib" )

		install ( FILES ${DRMAA_INSTALL_LOC}/lib/libdrmaa.dll DESTINATION ${C_BIN} )
	endif()

	ExternalProject_Add(drmaa
						#-- Download Step ----------
						DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${DRMAA_VER}.tar.gz ${CMD_TERM}
						DOWNLOAD_DIR ${DRMAA_STAGE}/dl
						URL http://parrot.cs.wisc.edu/externals/${DRMAA_VER}.tar.gz
						#--Configure step ----------
						CONFIGURE_COMMAND ${DRMAA_CONFIGURE}
						#--Build Step ----------
						BUILD_COMMAND ${DRMAA_MAKE}
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${DRMAA_INSTALL_LOC}
						INSTALL_COMMAND ${DRMAA_INSTALL})

	# if a local built copy exists disable from all build 
	if ( DRMAA_PREBUILT )  
		set_target_properties( drmaa PROPERTIES EXCLUDE_FROM_ALL TRUE)
	else()
		append_var(CONDOR_EXTERNALS drmaa)
	endif()
	
	message (STATUS "external configured (DRMAA_FOUND=${DRMAA_FOUND})")
	set( DRMAA_FOUND ${DRMAA_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_DRMAA ON PARENT_SCOPE )

	append_var(CONDOR_EXTERNAL_INCLUDE_DIRS ${DRMAA_INSTALL_LOC}/include)
	append_var(CONDOR_EXTERNAL_LINK_DIRS ${DRMAA_INSTALL_LOC}/lib)

	# i've tar bundled drmaa, does anyone actually use this?  if so I can explode.
	install ( FILES ${EXTERNAL_DL}/${DRMAA_VER}.tar.gz DESTINATION ${C_SRC}/drmaa )
	install ( FILES ${EXTERNAL_STAGE}/include/drmaa.h
			  ${EXTERNAL_STAGE}/include/README
			  DESTINATION ${C_INCLUDE} )
			  
	install ( FILES ${DRMAA_FOUND} DESTINATION ${C_LIB} )

else ()
	message ( STATUS "external skipped (drmaa)" )
endif ()
