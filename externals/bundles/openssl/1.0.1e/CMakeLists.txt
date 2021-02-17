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

		condor_post_external( openssl include OFF "${OPENSSL_FOUND}" )

		set(BUILDING_OPENSSL ON PARENT_SCOPE)

	else()

		if ( DARWIN )
			exec_program (sw_vers ARGS -productVersion OUTPUT_VARIABLE TEST_VER)
			if (${TEST_VER} MATCHES "10.1[1-2]" )
				if ( NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/openssl-darwin )
					if ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/openssl-darwin.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_SOURCE_DIR} ARGS xvf openssl-darwin.tar.gz )
					else()
						exec_program( curl ${CMAKE_CURRENT_BINARY_DIR} ARGS -O ${EXTERNALS_SOURCE_URL}/openssl-darwin.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf openssl-darwin.tar.gz )
					endif()
				endif()
				set( DARWIN_OPENSSL_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/openssl-darwin/include" )
				set( DARWIN_OPENSSL_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/openssl-darwin/lib" )
				set(BUILDING_OPENSSL ON PARENT_SCOPE)
			elseif (${TEST_VER} MATCHES "10.1[3-9]" )
				if ( NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7 )
					if ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libressl-2.2.7.tar.gz )
						message (STATUS "JEF using tarball in source dir")
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf ${CMAKE_CURRENT_SOURCE_DIR}/libressl-2.2.7.tar.gz libressl-2.2.7/include )
					else()
						exec_program( curl ${CMAKE_CURRENT_BINARY_DIR} ARGS -O ${EXTERNALS_SOURCE_URL}/libressl-2.2.7.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf libressl-2.2.7.tar.gz libressl-2.2.7/include )
					endif()
					exec_program( mkdir ${CMAKE_CURRENT_BINARY_DIR} ARGS libressl-2.2.7/lib )
					exec_program( ln ${CMAKE_CURRENT_BINARY_DIR} ARGS -s /usr/lib/libcrypto.35.dylib libressl-2.2.7/lib/libcrypto.dylib )
					exec_program( ln ${CMAKE_CURRENT_BINARY_DIR} ARGS -s /usr/lib/libssl.35.dylib libressl-2.2.7/lib/libssl.dylib )
				endif()
				set( DARWIN_OPENSSL_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7/include" )
				set( DARWIN_OPENSSL_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7/lib" )
				set(BUILDING_OPENSSL ON PARENT_SCOPE)
			elseif (${TEST_VER} MATCHES "11.[1-9]")
				if ( NOT EXISTS ${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7 )
					if ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libressl-2.2.7.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf ${CMAKE_CURRENT_SOURCE_DIR}/libressl-2.2.7.tar.gz libressl-2.2.7/include )
					else()
						exec_program( curl ${CMAKE_CURRENT_BINARY_DIR} ARGS -O ${EXTERNALS_SOURCE_URL}/libressl-2.2.7.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf libressl-2.2.7.tar.gz libressl-2.2.7/include )
					endif()
					if ( EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/libressl_libs-2.2.7.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf ${CMAKE_CURRENT_SOURCE_DIR}/libressl_libs-2.2.7.tar.gz )
					else()
						exec_program( curl ${CMAKE_CURRENT_BINARY_DIR} ARGS -O ${EXTERNALS_SOURCE_URL}/libressl_libs-2.2.7.tar.gz )
						exec_program( tar ${CMAKE_CURRENT_BINARY_DIR} ARGS xvf libressl_libs-2.2.7.tar.gz )
					endif()
				endif()
				set( DARWIN_OPENSSL_INCLUDE "${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7/include" )
				set( DARWIN_OPENSSL_LIB_DIR "${CMAKE_CURRENT_BINARY_DIR}/libressl-2.2.7/lib" )
				set(BUILDING_OPENSSL ON PARENT_SCOPE)
			else()
				set( DARWIN_OPENSSL_INCLUDE /usr/include )
				set( DARWIN_OPENSSL_LIB_DIR /usr/lib )
			endif()

			set( LIBSSL_FOUND "${DARWIN_OPENSSL_LIB_DIR}/libssl.dylib" )
			set( LIBCRYPTO_FOUND "${DARWIN_OPENSSL_LIB_DIR}/libcrypto.dylib" )
		else()
			find_library( LIBSSL_FOUND "ssl" )
			find_library( LIBCRYPTO_FOUND "crypto" )
		endif()

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
		set( DARWIN_OPENSSL_INCLUDE ${DARWIN_OPENSSL_INCLUDE} PARENT_SCOPE )
		set( DARWIN_OPENSSL_LIB_DIR ${DARWIN_OPENSSL_LIB_DIR} PARENT_SCOPE )
	endif(OPENSSL_FOUND)

else(WITH_OPENSSL)

	message (STATUS "external skipped (openssl)")

endif(WITH_OPENSSL)
