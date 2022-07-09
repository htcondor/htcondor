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

if (LINUX)
    option(WITH_GLOBUS "Compiling with support for GLOBUS" OFF)
endif()

if (WITH_GLOBUS)

	if (NOT PROPER)

		if (NOT HAVE_LIBLTDL AND NOT APPLE)
			message(FATAL_ERROR "Can't find libltdl, needed to build globus external!")
		endif()

		execute_process(COMMAND perl -e "use Archive::Tar"
				RESULT_VARIABLE TEST_RESULT )
		if(NOT ${TEST_RESULT} MATCHES "0")
			message(FATAL_ERROR "Missing perl module Archive::Tar, needed to build globus external!")
		endif()

		execute_process(COMMAND perl -e "use Digest::MD5"
				RESULT_VARIABLE TEST_RESULT )
		if(NOT ${TEST_RESULT} MATCHES "0")
			message(FATAL_ERROR "Missing perl module Digest::MD5, needed to build globus external!")
		endif()

		if(${SYS_ARCH} MATCHES "IA64|X86_64")
			set( GLOBUS_FLAVOR gcc64dbg )
		else ()
			set( GLOBUS_FLAVOR gcc32dbg )
		endif()

		condor_pre_external( GLOBUS globus-5.2.5-p1 "lib;include" "done")

		# GNU libtool isn't included in Mac OS 10.8, but Globus
		# needs libltdl. Build it and install it in the Globus
		# install location. Then add it to the include and link paths.
		# (For our 10.7 binaries to run on 10.6 or 10.8, we can't
		#  use the OS's libltdl. So always build our own on Darwin.)
		if ( APPLE )
			condor_pre_external( LIBTOOL libtool-2.4.2 "lib;include" "done")

			ExternalProject_Add(libtool
					#-- Download Step ----------
					DOWNLOAD_DIR ${LIBTOOL_STAGE}/dl
					URL ${EXTERNALS_SOURCE_URL}/libtool-2.4.2.tar.gz
					#--Configure step ----------
					CONFIGURE_COMMAND ./configure --prefix=${LIBTOOL_INSTALL_LOC}
					#--Build Step ----------
					BUILD_COMMAND make
					BUILD_IN_SOURCE 1
					#--install Step ----------
					INSTALL_DIR ${LIBTOOL_INSTALL_LOC}
					INSTALL_COMMAND make install &&
						touch ${LIBTOOL_INSTALL_LOC}/done)

			condor_post_external( libtool OFF OFF "" )
			set( GLOBUS_DEPENDS ${LIBTOOL_REF} )
			set( GLOBUS_FLAGS CPPFLAGS=-I${GLOBUS_INSTALL_LOC}/include LDFLAGS=-L${GLOBUS_INSTALL_LOC}/lib )
		else()
			set( GLOBUS_DEPENDS "" )
			set( GLOBUS_FLAGS "" )
		endif()


		set (GLOBUS_PATCH patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins.patch -p1 &&
						patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/gram_tls.patch -p1 &&
						patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/mac_openssl.patch -p1 &&
						cd source-trees/xio/src/builtins &&
						mkdir -p gsi popen &&
						cp ../../drivers/gsi/globus_i_xio_gsi.h gsi/globus_i_xio_gsi.h &&
						sed -i.old -e s/globus_xio_gsi.h/globus_xio_gsi_driver.h/ gsi/globus_i_xio_gsi.h &&
						cp ../../drivers/gsi/globus_xio_gsi.c gsi/globus_xio_gsi_driver.c &&
						cp ../../drivers/gsi/globus_xio_gsi.h gsi/globus_xio_gsi_driver.h &&
						cp ../../drivers/popen/source/globus_xio_popen_driver.h popen &&
						cp ../../drivers/popen/source/globus_xio_popen_driver.c popen &&
						cp tcp/Makefile.am gsi/Makefile.am &&
						sed -i.old -e s/tcp/gsi/g gsi/Makefile.am &&
						cp tcp/Makefile.in gsi/Makefile.in &&
						sed -i.old -e s/tcp/gsi/g gsi/Makefile.in &&
						cp tcp/Makefile.am popen/Makefile.am &&
						sed -i.old -e s/tcp/popen/g popen/Makefile.am &&
						cp tcp/Makefile.in popen/Makefile.in &&
						sed -i.old -e s/tcp/popen/g popen/Makefile.in )

		if(APPLE)
			set ( GLOBUS_PATCH ${GLOBUS_PATCH} &&
				cd ../../../../ &&
				patch -N -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins-2.patch -p1 &&
				ln -s ${DARWIN_OPENSSL_INCLUDE}/openssl ${GLOBUS_INSTALL_LOC}/include &&
				ln -s ${LIBTOOL_INSTALL_LOC}/include/ltdl.h ${GLOBUS_INSTALL_LOC}/include &&
				ln -s ${LIBTOOL_INSTALL_LOC}/include/libltdl ${GLOBUS_INSTALL_LOC}/include &&
				cp -a ${LIBTOOL_INSTALL_LOC}/lib/libltdl.7.dylib ${GLOBUS_INSTALL_LOC}/lib &&
				cp -a ${LIBTOOL_INSTALL_LOC}/lib/libltdl.a ${GLOBUS_INSTALL_LOC}/lib &&
				cp -a ${LIBTOOL_INSTALL_LOC}/lib/libltdl.dylib ${GLOBUS_INSTALL_LOC}/lib &&
				cp -a ${LIBTOOL_INSTALL_LOC}/lib/libltdl.la ${GLOBUS_INSTALL_LOC}/lib &&
				ln -s ${DARWIN_OPENSSL_LIB_DIR}/libcrypto.dylib ${GLOBUS_INSTALL_LOC}/lib &&
				ln -s ${DARWIN_OPENSSL_LIB_DIR}/libssl.dylib ${GLOBUS_INSTALL_LOC}/lib )
		endif()

		# honestly the build of globus is one of the strangest things I have ever seen and it makes no sense to me.
		ExternalProject_Add(globus
							DEPENDS ${GLOBUS_DEPENDS}
							#-- Download Step ----------
							DOWNLOAD_DIR ${GLOBUS_STAGE}/dl
							URL ${EXTERNALS_SOURCE_URL}/gt5.2.5-all-source-installer.tar.gz
							#--Patch step ----------
							PATCH_COMMAND ${GLOBUS_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND ln -s lib ${GLOBUS_INSTALL_LOC}/lib64 &&
							./configure --prefix=${GLOBUS_INSTALL_LOC} --with-flavor=${GLOBUS_FLAVOR}pthr
							#--Build Step ----------
							BUILD_COMMAND ${GLOBUS_FLAGS} make gpt globus_gssapi_error globus-resource-management-sdk globus-data-management-sdk &&
								cd ${GLOBUS_INSTALL_LOC}/include/globus/ && ln -sf ${GLOBUS_FLAVOR}pthr/globus_config.h .
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${GLOBUS_INSTALL_LOC}
							INSTALL_COMMAND find ${GLOBUS_INSTALL_LOC}/lib -name libssl.*dylib -delete &&
								find ${GLOBUS_INSTALL_LOC}/lib -name libcrypto.*dylib -delete &&
								touch ${GLOBUS_INSTALL_LOC}/done)

		## Set all the config information
		if ( LINUX )
			set( LIBGLOBUS_COMMON_SO "libglobus_common.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_CALLOUT_SO "libglobus_callout.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_PROXY_SSL_SO "libglobus_proxy_ssl.so.1" PARENT_SCOPE )
			set( LIBGLOBUS_OPENSSL_ERROR_SO "libglobus_openssl_error.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_OPENSSL_SO "libglobus_openssl.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_GSI_CERT_UTILS_SO "libglobus_gsi_cert_utils.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_OLDGAA_SO "libglobus_oldgaa.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_GSI_SYSCONFIG_SO "libglobus_gsi_sysconfig.so.1" PARENT_SCOPE )
			set( LIBGLOBUS_GSI_CALLBACK_SO "libglobus_gsi_callback.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_GSI_CREDENTIAL_SO "libglobus_gsi_credential.so.1" PARENT_SCOPE )
			set( LIBGLOBUS_GSI_PROXY_CORE_SO "libglobus_gsi_proxy_core.so.0" PARENT_SCOPE )
			set( LIBGLOBUS_GSSAPI_GSI_SO "libglobus_gssapi_gsi.so.4" PARENT_SCOPE )
			set( LIBGLOBUS_GSS_ASSIST_SO "libglobus_gss_assist.so.3" PARENT_SCOPE )

			set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.so;${HAVE_LIBLTDL}")
			set (GLOBUS_FOUND_STATIC "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.a;${HAVE_LIBLTDL}")
		elseif( APPLE )
			set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.dylib")
			set (GLOBUS_FOUND_STATIC "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.a;${GLOBUS_INSTALL_LOC}/lib/libltdl.a")
		else()
			set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.a")
			set (GLOBUS_FOUND_STATIC "${GLOBUS_FOUND}")
		endif()
			
		# primarily used by cream 
		foreach (_glob_lib ${GLOBUS_FOUND})
			string(REPLACE "${GLOBUS_FLAVOR}" "${GLOBUS_FLAVOR}pthr" _glob_thr_lib ${_glob_lib} )
			append_var(GLOBUS_THR_FOUND "${_glob_thr_lib}")
		endforeach(_glob_lib)

		if ( LINUX )
			set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.so")
		elseif ( APPLE )
			set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.dylib")
		else()
			set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.a")
		endif()
			
		if ( LINUX )
			set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_client.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.so")
		elseif ( APPLE )
			set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_client.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.dylib")
		else()
			set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_client.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.a")
		endif()

		condor_post_external( globus include/globus OFF "${GLOBUS_GRID_UNIVERSE_NORDUGRID};${GLOBUS_GRID_UNIVERSE_COMMON};${GLOBUS_FOUND}" )

		if ( LINUX )
			# Install the Globus libraries manually.
			# In order to use an install(FILES) command,
			# we'd have to explicitly list evey file.
			# We can't use a file(GLOB) because that's
			# evaluated when cmake is run, which may be
			# before the external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${GLOBUS_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
				COMMAND xargs -isrc cp -a --remove-destination src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.so*
					COMMAND xargs strip)" )
			endif()
		endif()
		if ( APPLE )
			# Install the Globus libraries manually.
			# In order to use an install(FILES) command,
			# we'd have to explicitly list evey file.
			# We can't use a file(GLOB) because that's
			# evaluated when cmake is run, which may be
			# before the external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${GLOBUS_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
				COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )

			# Fix up the share library dependencies
			install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.dylib*
				 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )

			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.dylib*
					COMMAND xargs strip -x)" )
			endif()
		endif()

	else( NOT PROPER )

		find_library( LIBGLOBUS_COMMON_FOUND "globus_common" )
		find_so_name( LIBGLOBUS_COMMON_SO ${LIBGLOBUS_COMMON_FOUND} )
		set( LIBGLOBUS_COMMON_SO ${LIBGLOBUS_COMMON_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_CALLOUT_FOUND "globus_callout" )
		find_so_name( LIBGLOBUS_CALLOUT_SO ${LIBGLOBUS_CALLOUT_FOUND} )
		set( LIBGLOBUS_CALLOUT_SO ${LIBGLOBUS_CALLOUT_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_PROXY_SSL_FOUND "globus_proxy_ssl" )
		find_so_name( LIBGLOBUS_PROXY_SSL_SO ${LIBGLOBUS_PROXY_SSL_FOUND} )
		set( LIBGLOBUS_PROXY_SSL_SO ${LIBGLOBUS_PROXY_SSL_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_OPENSSL_ERROR_FOUND "globus_openssl_error" )
		find_so_name( LIBGLOBUS_OPENSSL_ERROR_SO ${LIBGLOBUS_OPENSSL_ERROR_FOUND} )
		set( LIBGLOBUS_OPENSSL_ERROR_SO ${LIBGLOBUS_OPENSSL_ERROR_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_OPENSSL_FOUND "globus_openssl" )
		find_so_name( LIBGLOBUS_OPENSSL_SO ${LIBGLOBUS_OPENSSL_FOUND} )
		set( LIBGLOBUS_OPENSSL_SO ${LIBGLOBUS_OPENSSL_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSI_CERT_UTILS_FOUND "globus_gsi_cert_utils" )
		find_so_name( LIBGLOBUS_GSI_CERT_UTILS_SO ${LIBGLOBUS_GSI_CERT_UTILS_FOUND} )
		set( LIBGLOBUS_GSI_CERT_UTILS_SO ${LIBGLOBUS_GSI_CERT_UTILS_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_OLDGAA_FOUND "globus_oldgaa" )
		find_so_name( LIBGLOBUS_OLDGAA_SO ${LIBGLOBUS_OLDGAA_FOUND} )
		set( LIBGLOBUS_OLDGAA_SO ${LIBGLOBUS_OLDGAA_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSI_SYSCONFIG_FOUND "globus_gsi_sysconfig" )
		find_so_name( LIBGLOBUS_GSI_SYSCONFIG_SO ${LIBGLOBUS_GSI_SYSCONFIG_FOUND} )
		set( LIBGLOBUS_GSI_SYSCONFIG_SO ${LIBGLOBUS_GSI_SYSCONFIG_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSI_CALLBACK_FOUND "globus_gsi_callback" )
		find_so_name( LIBGLOBUS_GSI_CALLBACK_SO ${LIBGLOBUS_GSI_CALLBACK_FOUND} )
		set( LIBGLOBUS_GSI_CALLBACK_SO ${LIBGLOBUS_GSI_CALLBACK_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSI_CREDENTIAL_FOUND "globus_gsi_credential" )
		find_so_name( LIBGLOBUS_GSI_CREDENTIAL_SO ${LIBGLOBUS_GSI_CREDENTIAL_FOUND} )
		set( LIBGLOBUS_GSI_CREDENTIAL_SO ${LIBGLOBUS_GSI_CREDENTIAL_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSI_PROXY_CORE_FOUND "globus_gsi_proxy_core" )
		find_so_name( LIBGLOBUS_GSI_PROXY_CORE_SO ${LIBGLOBUS_GSI_PROXY_CORE_FOUND} )
		set( LIBGLOBUS_GSI_PROXY_CORE_SO ${LIBGLOBUS_GSI_PROXY_CORE_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSSAPI_GSI_FOUND "globus_gssapi_gsi" )
		find_so_name( LIBGLOBUS_GSSAPI_GSI_SO ${LIBGLOBUS_GSSAPI_GSI_FOUND} )
		set( LIBGLOBUS_GSSAPI_GSI_SO ${LIBGLOBUS_GSSAPI_GSI_SO} PARENT_SCOPE )
		find_library( LIBGLOBUS_GSS_ASSIST_FOUND "globus_gss_assist" )
		find_so_name( LIBGLOBUS_GSS_ASSIST_SO ${LIBGLOBUS_GSS_ASSIST_FOUND} )
		set( LIBGLOBUS_GSS_ASSIST_SO ${LIBGLOBUS_GSS_ASSIST_SO} PARENT_SCOPE )

		find_multiple( "globus_gss_assist;globus_gssapi_gsi;globus_gsi_proxy_core;globus_gsi_credential;globus_gsi_callback;globus_gsi_sysconfig;globus_oldgaa;globus_gsi_cert_utils;globus_openssl;globus_openssl_error;globus_proxy_ssl;globus_callout;globus_common;ltdl" GLOBUS_FOUND )
		find_multiple( "globus_gssapi_error;globus_xio;globus_io;globus_rsl" GLOBUS_GRID_UNIVERSE_COMMON )
		find_multiple( "globus_ftp_client" GLOBUS_GRID_UNIVERSE_NORDUGRID)
		if (GLOBUS_FOUND)
		  find_program(DPKG_ARCHITECTURE dpkg-architecture)
		  if (DPKG_ARCHITECTURE)
		    execute_process(
		      COMMAND ${DPKG_ARCHITECTURE} -qDEB_HOST_MULTIARCH
		      OUTPUT_VARIABLE DEB_HOST_MULTIARCH
		      OUTPUT_STRIP_TRAILING_WHITESPACE)
		  else()
		    set (DEB_HOST_MULTIARCH no-deb-multiarch)
		  endif()
		  append_var (CONDOR_EXTERNAL_INCLUDE_DIRS "/usr/include/globus;/usr/lib64/globus/include;/usr/lib/globus/include;/usr/include/${DEB_HOST_MULTIARCH}/globus;/usr/local/globus/include/globus")
		endif(GLOBUS_FOUND)

	endif(NOT PROPER)

	if (GLOBUS_FOUND)

		message( STATUS "external configured (GLOBUS_FOUND=${GLOBUS_FOUND})" )
		set( HAVE_EXT_GLOBUS ON PARENT_SCOPE )
		set( GLOBUS_FLAVOR ${GLOBUS_FLAVOR} PARENT_SCOPE )
		set( GLOBUS_FOUND ${GLOBUS_FOUND} PARENT_SCOPE )
		set( GLOBUS_FOUND_STATIC ${GLOBUS_FOUND_STATIC} PARENT_SCOPE )

		if (GLOBUS_GRID_UNIVERSE_COMMON)
			message( STATUS "(GLOBUS_GRID_UNIVERSE_COMMON=${GLOBUS_GRID_UNIVERSE_COMMON})" )
			set( GLOBUS_GRID_UNIVERSE_COMMON ${GLOBUS_GRID_UNIVERSE_COMMON} PARENT_SCOPE )
		endif(GLOBUS_GRID_UNIVERSE_COMMON)

		if (GLOBUS_GRID_UNIVERSE_NORDUGRID)
			message( STATUS "(GLOBUS_GRID_UNIVERSE_NORDUGRID=${GLOBUS_GRID_UNIVERSE_NORDUGRID})" )
			set( GLOBUS_GRID_UNIVERSE_NORDUGRID ${GLOBUS_GRID_UNIVERSE_NORDUGRID} PARENT_SCOPE )
		endif(GLOBUS_GRID_UNIVERSE_NORDUGRID)

	endif(GLOBUS_FOUND)

else( WITH_GLOBUS )

	message (STATUS "external skipped (globus)")

endif( WITH_GLOBUS )
