 ###############################################################
 # 
 # Copyright 2014 Red Hat, Inc. 
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

# The source code for the Condor drmaa library is hosted at
#   https://sourceforge.net/projects/condor-ext
# The source packaged as versions 1.6 and 1.6.1 in our externals
# repository is the latest code from Sourceforge as of 2009.
# As of February, 2014, no additional changes have been made upstream.
# Version 1.6.1 replaces the old Condor license when the Apache license.
# Version 1.6.2 fixes some handling of job exit signals.

if (NOT PROPER AND (LINUX OR WINDOWS OR DARWIN))
  option(WITH_DRMAA "Compiling with support for drmaa" ON)
endif()

if ( WITH_DRMAA )

	condor_pre_external( DRMAA drmaa-1.6.2-p1 "lib;include" "include/drmaa.h")
	
	if (NOT WINDOWS)
			set (DRMAA_CONFIGURE ./configure CFLAGS=-fcommon --prefix=${DRMAA_INSTALL_LOC})
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
		set (DRMAA_MAKE nmake /f libdrmaa.mak "CFG=libdrmaa - Win32 Release")
		set (DRMAA_INSTALL  cp Release/libdrmaa.lib Release/libdrmaa.dll ${DRMAA_INSTALL_LOC}/lib \r\n
		                    cp drmaa.h README ${DRMAA_INSTALL_LOC}/include)
			
		set( DRMAA_FOUND "${DRMAA_INSTALL_LOC}/lib/libdrmaa.lib" )

		install ( FILES ${DRMAA_INSTALL_LOC}/lib/libdrmaa.dll DESTINATION ${C_BIN} )
	endif()

	ExternalProject_Add(drmaa
						#-- Download Step ----------
						DOWNLOAD_DIR ${DRMAA_STAGE}/dl
						URL ${EXTERNALS_SOURCE_URL}/drmaa-1.6.2.tar.gz
						#--Patch step ----------
						PATCH_COMMAND patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/schedd_name.patch
						#--Configure step ----------
						CONFIGURE_COMMAND ${DRMAA_CONFIGURE}
						#--Build Step ----------
						BUILD_COMMAND ${DRMAA_MAKE}
						BUILD_IN_SOURCE 1
						#--install Step ----------
						INSTALL_DIR ${DRMAA_INSTALL_LOC}
						INSTALL_COMMAND ${DRMAA_INSTALL})

	# set standard globals include and link loc
	condor_post_external( drmaa include OFF "" )
	message (STATUS "external configured (DRMAA_FOUND=${DRMAA_FOUND})")
	
	# i've tar bundled drmaa, does anyone actually use this?  if so I can explode.
	install ( FILES ${DRMAA_STAGE}/dl/drmaa-1.6.2.tar.gz DESTINATION ${C_SRC}/drmaa )
	install ( FILES ${DRMAA_INSTALL_LOC}/include/drmaa.h
			  ${DRMAA_INSTALL_LOC}/include/README
			  DESTINATION ${C_INCLUDE} )
			  
	install ( FILES ${DRMAA_FOUND} DESTINATION ${C_LIB} )

else ()
	message ( STATUS "external skipped (drmaa)" )
endif ()
