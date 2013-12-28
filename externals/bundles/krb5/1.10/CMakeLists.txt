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


if (NOT HPUX)
  option(WITH_KRB5 "Compiling with support for KRB5" ON)
endif()
  
if (WITH_KRB5)

	find_multiple( "krb5;com_err;k5crypto;krb5support;gssapi_krb5" KRB5_FOUND )

  if ( NOT KRB5_FOUND AND NOT PROPER AND NOT LINUX AND NOT DARWIN )
  
    condor_pre_external( KRB5 krb5-1.10 "lib;include" "include/krb5.h")
    
    set (KRB5_DOWNLOAD ${KRB5_VER}.tar.gz)

		if (WINDOWS)

      if (MSVC11)
        if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
          set (KRB5_DOWNLOAD ${KRB5_VER}-VC11-Win64.tar.gz)
        else ()
          set (KRB5_DOWNLOAD ${KRB5_VER}-VC11-Win32.tar.gz)
        endif ()
      endif ()
      
			set (KRB5_CONFIGURE echo "No configuration necessary")
      set (KRB5_MAKE echo "No make necessary")

			# Something is wrong with the default install (make install suck-o)
			set (KRB5_INSTALL cp -r *.lib ${KRB5_INSTALL_LOC}/lib \r\n
                cp -r *.dll ${KRB5_INSTALL_LOC}/lib \r\n
							  cp -r include/* ${KRB5_INSTALL_LOC}/include \r\n)

		else()
			set (KRB5_PATCH  
				cp ${CMAKE_CURRENT_SOURCE_DIR}/src.appl.telnet.configure src/appl/telnet/configure ${CMD_TERM} 
				patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/eai_nodata.patch ${CMD_TERM}
				patch -N -p1 -i ${CMAKE_CURRENT_SOURCE_DIR}/getline.patch )

			set (KRB5_CONFIGURE src/configure --prefix=${KRB5_INSTALL_LOC} --without-tcl --enable-static --sysconfdir=/etc --disable-thread-support )
			set (KRB5_MAKE make )
			set (KRB5_INSTALL make install )
		endif()

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
		if (WINDOWS)
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
              ${KRB5_INSTALL_LOC}/lib/leashw64.dll
							${KRB5_INSTALL_LOC}/lib/wshelp64.dll
							${KRB5_INSTALL_LOC}/lib/xpprof64.dll
              DESTINATION ${C_LIB} )
      else ()
        install ( FILES ${KRB5_INSTALL_LOC}/lib/comerr32.dll
							${KRB5_INSTALL_LOC}/lib/gssapi32.dll
							${KRB5_INSTALL_LOC}/lib/k5sprt32.dll
							${KRB5_INSTALL_LOC}/lib/krb5_32.dll
              ${KRB5_INSTALL_LOC}/lib/krbcc32.dll
              ${KRB5_INSTALL_LOC}/lib/leashw32.dll
							${KRB5_INSTALL_LOC}/lib/wshelp32.dll
              ${KRB5_INSTALL_LOC}/lib/xpprof32.dll
              DESTINATION ${C_LIB} )
      endif ()
		else()
			set(KRB5_FOUND "${KRB5_INSTALL_LOC}/lib/libkrb5.a;${KRB5_INSTALL_LOC}/lib/libk5crypto.a;${KRB5_INSTALL_LOC}/lib/libcom_err.a;${KRB5_INSTALL_LOC}/lib/libkrb5support.a")
			if ( AIX )
				set (KRB5_FOUND "-Wl,-bstatic;${KRB5_FOUND};-Wl,-bdynamic")
			endif()
		endif()

		condor_post_external( krb5 include OFF )

	endif( )

	if (KRB5_FOUND)
		message (STATUS "external configured (KRB5_FOUND=${KRB5_FOUND})")
		set( KRB5_FOUND ${KRB5_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_KRB5 ON PARENT_SCOPE )
	endif()

else(WITH_KRB5)

	message (STATUS "external skipped (krb5)")

endif(WITH_KRB5)
