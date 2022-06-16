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

	find_multiple( "krb5;com_err;k5crypto;krb5support;gssapi_krb5" KRB5_FOUND )

	if ( NOT KRB5_FOUND AND NOT PROPER AND NOT LINUX AND NOT APPLE )

		condor_pre_external( KRB5 krb5-1.4.3-p1 "lib;include" "include/krb5.h")

		if (WINDOWS)
			# Just so it is noted this is a complete and total hack build of an arcane 
			# version of krb5, this is such a terrible practice it gives me chills.  b4 we 
			# used to error out and go about our marry way.  OMG.
			#
			# TJ: krb5 has _TONS_ of warnings but since we can't fix them
			# we'll just make them shut up instead. 
			#   4002 too many actual parameters for macro
			#   4005 macro redefinition
			#   4013 function undefined assuming extern returning int
			#   4018 signed/unsigned mismatch
			#   4028 formal parameter different from declaration
			#   4047 different in levels of indirection
			#   4090 different 'const' qualifiers
			#   4091 ignored on left of 'type' when no variable is declared
			#   4101 ureferenced local variable
			#   4102 unreferenced label
			#   4133 function : incompatible types
			#   4146 unary minus operator applied to unsigned type, result still unsigned.
			#   4244 conversion from 'type' to 'type', possible loss of data
			#   4700 uninitialized local variable
			#   4996 is use of obsolete posix names for functions

			set (KRB5_PATCH cp ${CMAKE_CURRENT_SOURCE_DIR}/win-mac.pre.h src/include/win-mac.h )
			set (KRB5_CONFIGURE cp ${CMAKE_CURRENT_SOURCE_DIR}/Makefile.in src/appl/gssftp/ftp/Makefile.in )
			set (KRB5_MAKE cd src \r\n
						   nmake -f Makefile.in prep-windows \r\n 
			               nmake NODEBUG=1 "DEFS=-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_WARNINGS -D_CRT_NON_CONFORMING_SWPRINTFS -wd4002 -wd4005 -wd4013 -wd4018 -wd4028 -wd4047 -wd4090 -wd4091 -wd4101 -wd4102 -wd4133 -wd4146 -wd4244 -wd4700 -wd4996")

			# Something is wrong with the default install (make install suck-o)
			set (KRB5_INSTALL cp -r src/lib/obj/i386/rel/* ${KRB5_INSTALL_LOC}/lib \r\n
							  cp -r src/include/* ${KRB5_INSTALL_LOC}/include \r\n
							  cp ${CMAKE_CURRENT_SOURCE_DIR}/win-mac.post.h  ${KRB5_INSTALL_LOC}/include/win-mac.h \r\n
							#  cd \"\\\" \r\n
							  cd /D "${KRB5_INSTALL_LOC}/lib" \r\n 
							  ${CMAKE_CURRENT_SOURCE_DIR}/manifest.bat "${KRB5_INSTALL_LOC}/lib" )

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
				    URL ${EXTERNALS_SOURCE_URL}/${KRB5_VER}.tar.gz
				    #URL C:/Scratch/Build/test-externals/${KRB5_VER}.tar.gz
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
			set(KRB5_FOUND "${KRB5_INSTALL_LOC}/lib/comerr32.lib;${KRB5_INSTALL_LOC}/lib/gssapi32.lib;${KRB5_INSTALL_LOC}/lib/k5sprt32.lib;${KRB5_INSTALL_LOC}/lib/krb5_32.lib;${KRB5_INSTALL_LOC}/lib/xpprof32.lib")

			#this may become the common case.
			install ( FILES ${KRB5_INSTALL_LOC}/lib/comerr32.dll
							${KRB5_INSTALL_LOC}/lib/gssapi32.dll
							${KRB5_INSTALL_LOC}/lib/k5sprt32.dll
							${KRB5_INSTALL_LOC}/lib/krb5_32.dll
							${KRB5_INSTALL_LOC}/lib/xpprof32.dll
					  DESTINATION ${C_LIB} )
		else()
			set(KRB5_FOUND "${KRB5_INSTALL_LOC}/lib/libkrb5.a;${KRB5_INSTALL_LOC}/lib/libk5crypto.a;${KRB5_INSTALL_LOC}/lib/libcom_err.a;${KRB5_INSTALL_LOC}/lib/libkrb5support.a")
			if ( AIX )
				set (KRB5_FOUND "-Wl,-bstatic;${KRB5_FOUND};-Wl,-bdynamic")
			endif()
		endif()

		condor_post_external( krb5 include OFF "" )

	endif( )

	if (KRB5_FOUND)
		message (STATUS "external configured (KRB5_FOUND=${KRB5_FOUND})")
		set( KRB5_FOUND ${KRB5_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_KRB5 ON PARENT_SCOPE )
	endif()

else(WITH_KRB5)

	message (STATUS "external skipped (krb5)")

endif(WITH_KRB5)
