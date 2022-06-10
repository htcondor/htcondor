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



option(WITH_CURL "Compiling with support for CURL" ON)

if (WITH_CURL)

	find_multiple( "curl" CURL_FOUND )

	if ( NOT CURL_FOUND AND NOT PROPER AND NOT LINUX AND NOT APPLE )
		
		condor_pre_external( CURL curl-7.33.0 "lib;include" "include/curl/curl.h")
    
    set (CURL_DOWNLOAD ${CURL_VER}.tar.gz)

		if (WINDOWS)
			set (CURL_CONFIGURE echo "Nothing to configure")
			set (CURL_MAKE echo "No make necessary")
			set (CURL_INSTALL cp -f *.lib ${CURL_INSTALL_LOC}/lib && cp -rf curl ${CURL_INSTALL_LOC}/include )
      
      if (MSVC11)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
          set (CURL_DOWNLOAD ${CURL_VER}-VC11-Win64.tar.gz)
        else ()
          set (CURL_DOWNLOAD ${CURL_VER}-VC11-Win32.tar.gz)
        endif ()
      endif ()
		else()
			set (CURL_PATCH echo "Nothing to patch on Unix")
			set (CURL_MAKE make)
			set (CURL_INSTALL make install)
			if ( BUILDING_OPENSSL )
				set (CURL_DEPENDS ${OPENSSL_REF})
				set (CURL_CONFIGURE PKG_CONFIG_PATH=${CURL_INSTALL_LOC} LIBS=-ldl ./configure --prefix=${CURL_INSTALL_LOC} --with-ssl=${OPENSSL_INSTALL_LOC} --disable-shared --disable-ipv6 --disable-ldap --disable-ldaps )
			else()
				set (CURL_DEPENDS "")
				set (CURL_CONFIGURE LIBS=-ldl ./configure --prefix=${CURL_INSTALL_LOC} --disable-shared --disable-ipv6 --disable-ldap --disable-ldaps )
			endif()
		endif()

		ExternalProject_Add(curl
							DEPENDS ${CURL_DEPENDS}
							#-- Download Step ----------
							DOWNLOAD_DIR ${CURL_STAGE}/dl
							URL ${EXTERNALS_SOURCE_URL}/${CURL_DOWNLOAD}
							#--Patch Step ----------
							PATCH_COMMAND ${CURL_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND ${CURL_CONFIGURE}
							#--Build Step ----------
							BUILD_COMMAND ${CURL_MAKE}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${CURL_INSTALL_LOC}
							INSTALL_COMMAND ${CURL_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(CURL_FOUND "${CURL_INSTALL_LOC}/lib/libcurl.lib")
		else()
			# libcurl is dependent on these libraries on some
			# platforms.
			find_multiple( "rt;idn" CURL_FOUND )
			set(CURL_FOUND "${CURL_INSTALL_LOC}/lib/libcurl.a;${CURL_FOUND}")
		endif()
		
		condor_post_external( curl include OFF )

	endif()

	if (CURL_FOUND)
		set( CURL_FOUND ${CURL_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_CURL ON PARENT_SCOPE )
		message (STATUS "external configured (CURL_FOUND=${CURL_FOUND})")
	endif(CURL_FOUND)

else(WITH_CURL)

	message (STATUS "external skipped (curl)")

endif(WITH_CURL)
