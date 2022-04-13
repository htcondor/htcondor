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

option(WITH_OPENSSL "Compiling with support for OPENSSL" ON)

if (WITH_OPENSSL)

  find_multiple( "ssl;crypto" OPENSSL_FOUND )

  if ( NOT OPENSSL_FOUND AND NOT PROPER AND NOT LINUX AND NOT DARWIN )

    condor_pre_external( OPENSSL openssl-1.1.1m "lib;include" "include/openssl/ssl.h")
    set (OPENSSL_DOWNLOAD ${OPENSSL_VER}.tar.gz)
    set (OPENSSL_DOWNLOAD_MD5SUM 8ec70f665c145c3103f6e330f538a9db)

    if (WINDOWS)

      set (OPENSSL_INSTALL cp lib/*.lib ${OPENSSL_INSTALL_LOC}/lib \r\n
        cp bin/*.dll ${OPENSSL_INSTALL_LOC}/lib \r\n
        cp -r include/openssl/ ${OPENSSL_INSTALL_LOC}/include \r\n )
      if (CMAKE_SIZEOF_VOID_P EQUAL 8)
        set (OPENSSL_DOWNLOAD ${OPENSSL_VER}-VC140-Win64.tar.gz)
        set (OPENSSL_DOWNLOAD_MD5SUM 3c110b43d8cfe0f49c36d91fe613ad65)
      else ()
        set (OPENSSL_DOWNLOAD ${OPENSSL_VER}-VC140-Win32.tar.gz)
        set (OPENSSL_DOWNLOAD_MD5SUM 979cd7ad777895a19b819fd4310aaa0b)
      endif ()

      set (OPENSSL_CONFIGURE echo "No configuration necessary")
      set (OPENSSL_MAKE echo "No make necessary")

    else(WINDOWS)
    endif(WINDOWS)

    ExternalProject_Add( openssl
      #-- Download Step ----------
      DOWNLOAD_DIR ${OPENSSL_STAGE}/dl
      URL ${EXTERNALS_SOURCE_URL}/${OPENSSL_DOWNLOAD}
      URL_MD5 ${OPENSSL_DOWNLOAD_MD5SUM}
      #--Patch step ----------
      PATCH_COMMAND ${OPENSSL_PATCH}
      #--Configure step ----------
      CONFIGURE_COMMAND ${OPENSSL_CONFIGURE}
      #--Build Step ----------
      BUILD_COMMAND ${OPENSSL_MAKE}
      BUILD_IN_SOURCE 1
      #--install Step ----------
      INSTALL_DIR ${OPENSSL_INSTALL_LOC}
      INSTALL_COMMAND ${OPENSSL_INSTALL} )

    if (WINDOWS)
      set(OPENSSL_FOUND "${OPENSSL_INSTALL_LOC}/lib/libssl.lib;${OPENSSL_INSTALL_LOC}/lib/libcrypto.lib")

      #this may become the common case.
      install ( FILES
        ${OPENSSL_INSTALL_LOC}/lib/libcrypto-1_1-x64.dll
        ${OPENSSL_INSTALL_LOC}/lib/libssl-1_1-x64.dll
        DESTINATION ${C_LIB} )
    else ()
      set(OPENSSL_FOUND "${OPENSSL_INSTALL_LOC}/lib/libssl.a;${OPENSSL_INSTALL_LOC}/lib/libcrypto.a")
    endif ()

    condor_post_external( openssl include OFF "${OPENSSL_FOUND}" )

		set(BUILDING_OPENSSL ON PARENT_SCOPE)

	endif( )

	if (OPENSSL_FOUND)
		message (STATUS "external configured (OPENSSL_FOUND=${OPENSSL_FOUND})")
		set( OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE )
		set( OPENSSL_INSTALL_LOC ${OPENSSL_INSTALL_LOC} PARENT_SCOPE )
		set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )
	endif(OPENSSL_FOUND)

else(WITH_OPENSSL)

	message (STATUS "external skipped (openssl)")

endif(WITH_OPENSSL)
