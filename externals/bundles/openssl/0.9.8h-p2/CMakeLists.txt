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

	find_multiple( "ssl;crypto" OPENSSL_FOUND )

	if ( NOT OPENSSL_FOUND AND NOT PROPER AND NOT LINUX AND NOT DARWIN )

		condor_pre_external( OPENSSL openssl-0.9.8h "lib;include" "include/openssl/ssl.h")

		if (WINDOWS)
			set (OPENSSL_PATCH patch -N --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/renegotiate.patch \r\n
							   patch -N --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/mk1mf.pl-0.9.8h-patch \r\n
							   patch -N --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/VC-32.pl-0.9.8h-patch )
			set (OPENSSL_CONFIGURE perl Configure VC-WIN32 )
			set (OPENSSL_MAKE  call ms/do_nt.bat \r\n
							   nmake -f ms/ntdll.mak )
			set (OPENSSL_INSTALL cp out32dll/*.lib ${OPENSSL_INSTALL_LOC}/lib \r\n
								 cp out32dll/*.dll ${OPENSSL_INSTALL_LOC}/lib \r\n
								 cp -r inc32/* ${OPENSSL_INSTALL_LOC}/include \r\n
								 cp ${CMAKE_CURRENT_SOURCE_DIR}/ossl_typ-post.h ${OPENSSL_INSTALL_LOC}/include/openssl/ossl_typ.h )
		else(WINDOWS)
			set (OPENSSL_PATCH patch -N --binary -p0 -i ${CMAKE_CURRENT_SOURCE_DIR}/renegotiate.patch &&
							   sed 's/-fomit-frame-pointer//' Configure > Configure.new &&
							   mv Configure.new Configure &&
							   chmod a+rx Configure )

			set (OPENSSL_CONFIGURE ./config --prefix=${OPENSSL_INSTALL_LOC} no-asm )
			
			if (DARWIN)
			  string(SUBSTRING ${OS_VER} 0 2 DARWIN_MAJOR)
			  if(${DARWIN_MAJOR} STREQUAL "10")
				dprint("OPENSSL_CONVERT")
				set (OPENSSL_CONFIGURE ./Configure darwin64-x86_64-cc --prefix=${OPENSSL_INSTALL_LOC} no-asm -fPIC )
			  endif()
			endif()
			 
			if ( NOT AIX )
				set (OPENSSL_CONFIGURE ${OPENSSL_CONFIGURE} -fPIC)
			endif()
			set (OPENSSL_MAKE make )
			set (OPENSSL_INSTALL make install )
		endif(WINDOWS)

		ExternalProject_Add( openssl
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${OPENSSL_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/${OPENSSL_VER}.tar.gz
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

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(OPENSSL_FOUND "${OPENSSL_INSTALL_LOC}/lib/libeay32.lib;${OPENSSL_INSTALL_LOC}/lib/ssleay32.lib")

			#this may become the common case.
			install ( FILES ${OPENSSL_INSTALL_LOC}/lib/libeay32.dll
							${OPENSSL_INSTALL_LOC}/lib/ssleay32.dll
					  DESTINATION ${C_LIB} )

		else(WINDOWS)
			set(OPENSSL_FOUND "${OPENSSL_INSTALL_LOC}/lib/libssl.a;${OPENSSL_INSTALL_LOC}/lib/libcrypto.a")
		endif(WINDOWS)

		condor_post_external( openssl include OFF "${OPENSSL_FOUND}" )

		set(BUILDING_OPENSSL ON PARENT_SCOPE)

	endif( )

	if (OPENSSL_FOUND)
		message (STATUS "external configured (OPENSSL_FOUND=${OPENSSL_FOUND})")
		set( OPENSSL_FOUND ${OPENSSL_FOUND} PARENT_SCOPE )
		set( OPENSSL_INSTALL_LOC ${OPENSSL_INSTALL_LOC} PARENT_SCOPE )
		set( HAVE_EXT_OPENSSL ON PARENT_SCOPE )
		set( HAVE_OPENSSL_SSL_H ON PARENT_SCOPE )
	endif(OPENSSL_FOUND)

else(WITH_OPENSSL)

	message (STATUS "external skipped (openssl)")

endif(WITH_OPENSSL)
