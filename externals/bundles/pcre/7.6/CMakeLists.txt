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

	condor_pre_external( PCRE pcre-7.6 "lib;include" "include/pcre.h")

	if (WINDOWS)
		set (PCRE_PATCH cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.win32 . \r\n
						cp ${CMAKE_CURRENT_SOURCE_DIR}/pcre.h.win32 pcre.h \r\n
						cp ${CMAKE_CURRENT_SOURCE_DIR}/libpcre.def.win32 libpcre.def )
		set (PCRE_CONFIGURE cp ${CMAKE_CURRENT_SOURCE_DIR}/config.h.win32 config.h)
		set (PCRE_MAKE gmake -f Makefile.win32 )
		set (PCRE_INSTALL cp libpcre.lib ${PCRE_INSTALL_LOC}/lib \r\n
						  cp libpcre.dll ${PCRE_INSTALL_LOC}/lib \r\n
						  cp pcre.h ${PCRE_INSTALL_LOC}/include )
	else()
# I believe we only neede the longlong patch on HPUX, but not willing to turn it off and potentially
# break other stuff just yet...
		set (PCRE_PATCH  patch -N < ${CMAKE_CURRENT_SOURCE_DIR}/longlong.patch &&
						 patch -N < ${CMAKE_CURRENT_SOURCE_DIR}/unittests.patch )
		set (PCRE_CONFIGURE ./configure --prefix=${PCRE_INSTALL_LOC} --disable-cpp )
		set (PCRE_MAKE make && make test )
		set (PCRE_INSTALL make install )
	endif()

	ExternalProject_Add(pcre
						#-- Download Step ----------
						DOWNLOAD_DIR ${PCRE_STAGE}/dl
						URL ${EXTERNALS_SOURCE_URL}/${PCRE_VER}.tar.gz
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
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.lib")
		install ( FILES ${PCRE_INSTALL_LOC}/lib/libpcre.dll DESTINATION ${C_LIB} )
	elseif(DARWIN)
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.dylib")
	else()
		set(PCRE_FOUND "${PCRE_INSTALL_LOC}/lib/libpcre.a")
	endif()
	#
	#################################################################

	condor_post_external( pcre include OFF )

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

##################### 
## Hard failure if there is no PCRE
if (PCRE_FOUND)
	message (STATUS "external configured (PCRE_FOUND=${PCRE_FOUND})")
	set( PCRE_FOUND ${PCRE_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_PCRE ON PARENT_SCOPE )
else()
	message (FATAL_ERROR "pcre not found and is required to build")
endif(PCRE_FOUND)

