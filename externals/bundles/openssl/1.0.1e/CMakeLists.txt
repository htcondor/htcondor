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

option(WITH_OPENSSL "Compiling with support for OPENSSL" ON)

if (WITH_OPENSSL)

	if ( WINDOWS )
  
		condor_pre_external( OPENSSL openssl-1.0.1e "lib;include" "include/openssl/ssl.h")

		set (OPENSSL_INSTALL cp *.lib ${OPENSSL_INSTALL_LOC}/lib \r\n
			cp *.dll ${OPENSSL_INSTALL_LOC}/lib \r\n
			cp -r openssl/ ${OPENSSL_INSTALL_LOC}/include \r\n )
			#cp ${CMAKE_CURRENT_SOURCE_DIR}/ossl_typ-post.h ${OPENSSL_INSTALL_LOC}/include/openssl/ossl_typ.h )
      
		if (MSVC11)
			if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
				set (OPENSSL_DOWNLOAD ${OPENSSL_VER}-VC11-Win64.tar.gz)
			else ()
				set (OPENSSL_DOWNLOAD ${OPENSSL_VER}-VC11-Win32.tar.gz)
			endif ()
		endif ()
      
		set (OPENSSL_CONFIGURE echo "No configuration necessary")
		set (OPENSSL_MAKE echo "No make necessary")
      
		ExternalProject_Add( openssl
			#-- Download Step ----------
			DOWNLOAD_DIR ${OPENSSL_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/${OPENSSL_DOWNLOAD}
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
    
		set(OPENSSL_FOUND "${OPENSSL_INSTALL_LOC}/lib/libeay32.lib;${OPENSSL_INSTALL_LOC}/lib/ssleay32.lib")

		#this may become the common case.
		install ( FILES ${OPENSSL_INSTALL_LOC}/lib/libeay32.dll
			${OPENSSL_INSTALL_LOC}/lib/ssleay32.dll
			DESTINATION ${C_LIB} )

		condor_post_external( openssl include OFF )

		set(BUILDING_OPENSSL ON PARENT_SCOPE)

	else()

		find_library( LIBSSL_FOUND "ssl" )
		find_library( LIBCRYPTO_FOUND "crypto" )
		set( OPENSSL_FOUND "${LIBSSL_FOUND};${LIBCRYPTO_FOUND}" )

		if( DLOPEN_SECURITY_LIBS )
			find_so_name( LIBSSL_SO ${LIBSSL_FOUND} )
			set( OPENSSL_FOUND "${LIBCRYPTO_FOUND}" )
		endif()

	endif( )

	if (OPENSSL_FOUND)
		message (STATUS "external configured (OPENSSL_FOUND=${OPENSSL_FOUND})")
		set( OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE )
		set( OPENSSL_INSTALL_LOC ${OPENSSL_INSTALL_LOC} PARENT_SCOPE )
		set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )
		set( HAVE_OPENSSL_SSL_H ON PARENT_SCOPE )
		set( LIBSSL_SO ${LIBSSL_SO} PARENT_SCOPE )
		set( LIBSSL_FOUND "${LIBSSL_FOUND}" PARENT_SCOPE )
		set( LIBCRYPTO_FOUND "${LIBCRYPTO_FOUND}" PARENT_SCOPE )
	endif(OPENSSL_FOUND)

else(WITH_OPENSSL)

	message (STATUS "external skipped (openssl)")

endif(WITH_OPENSSL)
