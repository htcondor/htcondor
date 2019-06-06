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

if ( NOT PROPER AND NOT LINUX )

  if (WINDOWS)
    condor_pre_external( PCRE pcre-8.40 "lib;include" "include/pcre.h")
		set (PCRE_INSTALL cp pcre.lib ${PCRE_INSTALL_LOC}/lib \r\n
						  cp pcre.dll ${PCRE_INSTALL_LOC}/lib \r\n
						  cp pcre.h ${PCRE_INSTALL_LOC}/include )
    
    if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
       set (PCRE_DOWNLOAD ${PCRE_VER}-VC15-Win64.tar.gz)
    else ()
       set (PCRE_DOWNLOAD ${PCRE_VER}-VC15-Win32.tar.gz)
    endif ()
    
    set (PCRE_CONFIGURE echo "No configuration necessary")
    set (PCRE_MAKE echo "No make necessary")
  else()
    set (PCRE_DOWNLOAD ${PCRE_VER}.tar.gz)
    # Currently not attempting to deal with newer version of PCRE on other platforms
  endif()

	ExternalProject_Add(pcre
    #-- Download Step ----------
    DOWNLOAD_DIR ${PCRE_STAGE}/dl
    URL ${EXTERNALS_SOURCE_URL}/${PCRE_DOWNLOAD}
    #--Patch step ----------
    PATCH_COMMAND ${PCRE_PATCH}
    #--Configure step ----------
    CONFIGURE_COMMAND ${PCRE_CONFIGURE}
    #--Build Step ----------
    BUILD_COMMAND ${PCRE_MAKE}
    BUILD_IN_SOURCE 1
    #--install Step ----------
    INSTALL_DIR ${PCRE_INSTALL_LOC}
    INSTALL_COMMAND ${PCRE_INSTALL} )

	#################################################################
	# Set the target dependencies which the rest of condor depends on.
	if (WINDOWS)
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/pcre.lib")
		install ( FILES ${PCRE_INSTALL_LOC}/lib/pcre.dll DESTINATION ${C_LIB} )
	elseif(DARWIN)
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.dylib")
	else()
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.a")
	endif()
	#
	#################################################################

	condor_post_external( pcre include OFF "${PCRE_FOUND}" )

	if ( DARWIN )
		# Install the PCRE libraries manually.
		# In order to use an install(FILES) command, we'd
		# have to explicitly list evey file.
		# We can't use a file(GLOB) because that's evaluated
		# when cmake is run, which may be before the
		# external is built.
		install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
		install( CODE "execute_process(COMMAND find ${PCRE_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
			COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
		if ( CONDOR_STRIP_PACKAGES )
			# Strip the libraries manually.
			# Since cmake didn't create the libraries
			# itself, it doesn't know to strip them.
			install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libpcre*.dylib*
				COMMAND xargs strip -x)" )
		endif()
	endif()

	set( HAVE_PCRE_H ON PARENT_SCOPE )

else( NOT PROPER AND NOT LINUX )

		if (HAVE_PCRE_H OR HAVE_PCRE_PCRE_H)
			find_multiple( "pcre" PCRE_FOUND )
		endif()

endif( NOT PROPER AND NOT LINUX )


if( SYSTEM_NAME STREQUAL "sl6.3" )
	condor_pre_external( PCRE pcre-8.40 "lib;include" "include/pcre.h")

	set( PCRE_DOWNLOAD ${PCRE_VER}.tar.gz )
	set( PCRE_CONFIGURE ./configure --prefix=${PCRE_INSTALL_LOC} --disable-cpp )
	set( PCRE_MAKE make && make test )
	set( PCRE_INSTALL make install )

	ExternalProject_Add(pcre
		DOWNLOAD_DIR ${PCRE_STAGE}/dl
		URL ${EXTERNALS_SOURCE_URL}/${PCRE_DOWNLOAD}
		PATCH_COMMAND ${PCRE_PATCH}
		CONFIGURE_COMMAND ${PCRE_CONFIGURE}
		BUILD_COMMAND ${PCRE_MAKE}
		BUILD_IN_SOURCE 1
		INSTALL_DIR ${PCRE_INSTALL_LOC}
		INSTALL_COMMAND ${PCRE_INSTALL}
	)

	set( PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.so" )
	condor_post_external( pcre include OFF "${PCRE_FOUND}" )

	set( HAVE_PCRE_H ON PARENT_SCOPE )
endif()

#####################
## Hard failure if there is no PCRE
if (PCRE_FOUND)
	message (STATUS "external configured (PCRE_FOUND=${PCRE_FOUND})")
	set( PCRE_FOUND ${PCRE_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_PCRE ON PARENT_SCOPE )
else()
	message (FATAL_ERROR "pcre not found and is required to build")
endif(PCRE_FOUND)
