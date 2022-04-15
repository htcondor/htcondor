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

if ( NOT PROPER AND NOT LINUX )

  if (WINDOWS)
    condor_pre_external( PCRE2 pcre2-10.39 "lib;include" "include/pcre.h")
		set (PCRE2_INSTALL cp pcre2-8.lib ${PCRE2_INSTALL_LOC}/lib \r\n
						  cp pcre2-8.dll ${PCRE2_INSTALL_LOC}/lib \r\n
						  cp pcre2.h ${PCRE2_INSTALL_LOC}/include )

	set (PCRE2_DOWNLOAD ${PCRE2_VER}-VC140-Win64.tar.gz)
	set (PCRE2_DOWNLOAD_MD5SUM 8e75a8000d3f6c3302b2f1918430e6bc}

    set (PCRE2_CONFIGURE echo "No configuration necessary")
    set (PCRE2_MAKE echo "No make necessary")
  else()
    set (PCRE2_DOWNLOAD ${PCRE2_VER}.tar.gz)
    set (PCRE2_DOWNLOAD_MD5SUM 7389e3524de2cda3d21fde8c224febf1}
    # Currently not attempting to deal with newer version of PCRE on other platforms
  endif()

	ExternalProject_Add(pcre
    #-- Download Step ----------
    DOWNLOAD_DIR ${PCRE2_STAGE}/dl
    URL ${EXTERNALS_SOURCE_URL}/${PCRE2_DOWNLOAD}
    URL_MD5 ${PCRE2_DOWNLOAD_MD5SUM}
    #--Patch step ----------
    PATCH_COMMAND ${PCRE2_PATCH}
    #--Configure step ----------
    CONFIGURE_COMMAND ${PCRE2_CONFIGURE}
    #--Build Step ----------
    BUILD_COMMAND ${PCRE2_MAKE}
    BUILD_IN_SOURCE 1
    #--install Step ----------
    INSTALL_DIR ${PCRE2_INSTALL_LOC}
    INSTALL_COMMAND ${PCRE2_INSTALL} )

	#################################################################
	# Set the target dependencies which the rest of condor depends on.
	if (WINDOWS)
		set(PCRE2_FOUND "${PCRE2_INSTALL_LOC}/lib/pcre2-8.lib")
		install ( FILES ${PCRE2_INSTALL_LOC}/lib/pcre2-8.dll DESTINATION ${C_LIB} )
	elseif(DARWIN)
		set(PCRE2_FOUND "${PCRE2_INSTALL_LOC}/lib/libpcre2-8.dylib")
	else()
		set(PCRE2_FOUND "${PCRE2_INSTALL_LOC}/lib/libpcre2-8.a")
	endif()
	#
	#################################################################

	condor_post_external( pcre include OFF "${PCRE2_FOUND}" )

	if ( DARWIN )
		# Install the PCRE libraries manually.
		# In order to use an install(FILES) command, we'd
		# have to explicitly list evey file.
		# We can't use a file(GLOB) because that's evaluated
		# when cmake is run, which may be before the
		# external is built.
		install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
		install( CODE "execute_process(COMMAND find ${PCRE2_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
			COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
		if ( CONDOR_STRIP_PACKAGES )
			# Strip the libraries manually.
			# Since cmake didn't create the libraries
			# itself, it doesn't know to strip them.
			install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libpcre*.dylib*
				COMMAND xargs strip -x)" )
		endif()
	endif()

	set( HAVE_PCRE2_H ON PARENT_SCOPE )

else( NOT PROPER AND NOT LINUX )

		if (HAVE_PCRE2_H OR HAVE_PCRE2_PCRE2_H)
			find_multiple( "pcre2" PCRE2_FOUND )
		endif()

endif( NOT PROPER AND NOT LINUX )


#####################
## Hard failure if there is no PCRE
if (PCRE2_FOUND)
	message (STATUS "external configured (PCRE2_FOUND=${PCRE2_FOUND})")
	set( PCRE2_FOUND ${PCRE2_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_PCRE ON PARENT_SCOPE )
else()
	message (FATAL_ERROR "pcre not found and is required to build")
endif(PCRE2_FOUND)
