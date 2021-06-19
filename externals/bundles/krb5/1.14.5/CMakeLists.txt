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


option(WITH_KRB5 "Compiling with support for KRB5" ON)
  
if (WITH_KRB5)

	if ( WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8 )
		find_multiple( "krb5_64;comerr64;gssapi64;xpprof64" KRB5_FOUND )
	elseif ( WINDOWS )
		find_multiple( "krb5_32;comerr32;gssapi32;xpprof32" KRB5_FOUND )
	else ( WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8 )
		find_multiple( "krb5;com_err;k5crypto;krb5support;gssapi_krb5" KRB5_FOUND )
	endif ( WINDOWS AND CMAKE_SIZEOF_VOID_P EQUAL 8 )

	if ( NOT KRB5_FOUND AND WINDOWS )
  
		condor_pre_external( KRB5 krb5-1.14.5 "lib;include" "include/krb5.h")
    
		set (KRB5_DOWNLOAD ${KRB5_VER}.tar.gz)

		if (CMAKE_SIZEOF_VOID_P EQUAL 8)
		   set (KRB5_DOWNLOAD ${KRB5_VER}-VC15-Win64.tar.gz)
		else ()
			set (KRB5_DOWNLOAD ${KRB5_VER}-VC15-Win32.tar.gz)
		endif ()

		set (KRB5_CONFIGURE echo "No configuration necessary")
		set (KRB5_MAKE echo "No make necessary")

		# Something is wrong with the default install (make install suck-o)
		set (KRB5_INSTALL cp -r *.lib ${KRB5_INSTALL_LOC}/lib \r\n
			cp -r *.dll ${KRB5_INSTALL_LOC}/lib \r\n
			cp -r include/* ${KRB5_INSTALL_LOC}/include \r\n)


			ExternalProject_Add(krb5
				#-- Download Step ----------
				DOWNLOAD_DIR ${KRB5_STAGE}/dl
				URL ${EXTERNALS_SOURCE_URL}/${KRB5_DOWNLOAD}
				#--Patch step ----------
				PATCH_COMMAND ${KRB5_PATCH}
				#--Configure step ----------
				CONFIGURE_COMMAND ${KRB5_CONFIGURE}
				#--Build Step ----------
				BUILD_COMMAND ${KRB5_MAKE}
				BUILD_IN_SOURCE 1
				#--install Step ----------
				INSTALL_DIR ${KRB5_INSTALL_LOC}
				INSTALL_COMMAND ${KRB5_INSTALL} )

			# Set the target dependencies which the rest of condor depends on.
			if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
				set(KRB5_FOUND "${KRB5_INSTALL_LOC}/lib/comerr64.lib;${KRB5_INSTALL_LOC}/lib/gssapi64.lib;${KRB5_INSTALL_LOC}/lib/krb5_64.lib;${KRB5_INSTALL_LOC}/lib/xpprof64.lib")
			else ()
				set(KRB5_FOUND "${KRB5_INSTALL_LOC}/lib/comerr32.lib;${KRB5_INSTALL_LOC}/lib/gssapi32.lib;${KRB5_INSTALL_LOC}/lib/krb5_32.lib;${KRB5_INSTALL_LOC}/lib/xpprof32.lib")
			endif ()

			#this may become the common case.
			if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
				install ( FILES ${KRB5_INSTALL_LOC}/lib/comerr64.dll
					${KRB5_INSTALL_LOC}/lib/gssapi64.dll
					${KRB5_INSTALL_LOC}/lib/k5sprt64.dll
					${KRB5_INSTALL_LOC}/lib/krb5_64.dll
					${KRB5_INSTALL_LOC}/lib/krbcc64.dll
#					${KRB5_INSTALL_LOC}/lib/leashw64.dll
					${KRB5_INSTALL_LOC}/lib/wshelp64.dll
					${KRB5_INSTALL_LOC}/lib/xpprof64.dll
					DESTINATION ${C_LIB} )
			else ()
				install ( FILES ${KRB5_INSTALL_LOC}/lib/comerr32.dll
					${KRB5_INSTALL_LOC}/lib/gssapi32.dll
					${KRB5_INSTALL_LOC}/lib/k5sprt32.dll
					${KRB5_INSTALL_LOC}/lib/krb5_32.dll
					${KRB5_INSTALL_LOC}/lib/krbcc32.dll
#					${KRB5_INSTALL_LOC}/lib/leashw32.dll
					${KRB5_INSTALL_LOC}/lib/wshelp32.dll
					${KRB5_INSTALL_LOC}/lib/xpprof32.dll
					DESTINATION ${C_LIB} )
			endif ()

		condor_post_external( krb5 include OFF "" )

	endif( )

	if( DLOPEN_SECURITY_LIBS )
		find_library( LIBCOM_ERR_PATH "com_err" )
		find_so_name( LIBCOM_ERR_SO ${LIBCOM_ERR_PATH} )
		find_library( LIBKRB5SUPPORT_PATH "krb5support" )
		find_so_name( LIBKRB5SUPPORT_SO ${LIBKRB5SUPPORT_PATH} )
		find_library( LIBK5CRYPTO_PATH "k5crypto" )
		find_so_name( LIBK5CRYPTO_SO ${LIBK5CRYPTO_PATH} )
		find_library( LIBKRB5_PATH "krb5" )
		find_so_name( LIBKRB5_SO ${LIBKRB5_PATH} )
		find_library( LIBGSSAPI_KRB5_PATH "gssapi_krb5" )
		find_so_name( LIBGSSAPI_KRB5_SO ${LIBGSSAPI_KRB5_PATH} )
		set( KRB5_FOUND "" )
	endif()

	message (STATUS "external configured (KRB5_FOUND=${KRB5_FOUND})")
	set( KRB5_FOUND ${KRB5_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_KRB5 ON PARENT_SCOPE )
	set( LIBCOM_ERR_SO ${LIBCOM_ERR_SO} PARENT_SCOPE )
	set( LIBKRB5SUPPORT_SO ${LIBKRB5SUPPORT_SO} PARENT_SCOPE )
	set( LIBK5CRYPTO_SO ${LIBK5CRYPTO_SO} PARENT_SCOPE )
	set( LIBKRB5_SO ${LIBKRB5_SO} PARENT_SCOPE )
	set( LIBGSSAPI_KRB5_SO ${LIBGSSAPI_KRB5_SO} PARENT_SCOPE )

else(WITH_KRB5)

	message (STATUS "external skipped (krb5)")

endif(WITH_KRB5)
