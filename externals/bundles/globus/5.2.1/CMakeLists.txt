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

if (LINUX OR DARWIN)
	option(WITH_GLOBUS "Compiling with support for GLOBUS" ON)
endif()

if (WITH_GLOBUS)

	if (NOT PROPER)

		if(${SYS_ARCH} MATCHES "IA64|X86_64")
			set( GLOBUS_FLAVOR gcc64dbg )
		elseif(AIX)
			set( GLOBUS_FLAVOR vendorcc32dbg )
		else ()
			set( GLOBUS_FLAVOR gcc32dbg )
		endif()

		condor_pre_external( GLOBUS globus-5.2.1-p1 "lib;include" "done")

		# GNU libtool isn't included in Mac OS 10.8, but Globus
		# needs libltdl. Build it and install it in the Globus
		# install location. Then add it to the include and link paths.
		# (For our 10.7 binaries to run on 10.6 or 10.8, we can't
		#  use the OS's libltdl. So always build our own on Darwin.)
		if ( DARWIN )
			condor_pre_external( LIBTOOL libtool-2.4.2 "lib;include" "done")

			ExternalProject_Add(libtool
					#-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/libtool-2.4.2.tar.gz ${CMD_TERM}
					DOWNLOAD_DIR ${LIBTOOL_STAGE}/dl
					URL http://parrot.cs.wisc.edu/externals/libtool-2.4.2.tar.gz
					#--Configure step ----------
					CONFIGURE_COMMAND cd libtool-2.4.2 && ./configure --prefix=${GLOBUS_INSTALL_LOC}
					#--Build Step ----------
					BUILD_COMMAND cd libtool-2.4.2 && make
					BUILD_IN_SOURCE 1
					#--install Step ----------
					INSTALL_DIR ${GLOBUS_INSTALL_LOC}
					INSTALL_COMMAND cd libtool-2.4.2 && make install )

			condor_post_external( libtool OFF OFF )
			set( GLOBUS_DEPENDS ${LIBTOOL_REF} )
			set( GLOBUS_FLAGS CPPFLAGS=-I${GLOBUS_INSTALL_LOC}/include LDFLAGS=-L${GLOBUS_INSTALL_LOC}/lib )
		else()
			set( GLOBUS_DEPENDS "" )
			set( GLOBUS_FLAGS "" )
		endif()


		set (GLOBUS_PATCH patch -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins.patch -p0 &&
						patch -i ${CMAKE_CURRENT_SOURCE_DIR}/mac_openssl.patch -p0 &&
						cd gt5.2.1-all-source-installer/source-trees/xio/src/builtins &&
						mkdir -p gsi popen &&
						sed -e 's/globus_xio_gsi.h/globus_xio_gsi_driver.h/' ../../drivers/gsi/globus_i_xio_gsi.h >gsi/globus_i_xio_gsi.h &&
						cp ../../drivers/gsi/globus_xio_gsi.c gsi/globus_xio_gsi_driver.c &&
						cp ../../drivers/gsi/globus_xio_gsi.h gsi/globus_xio_gsi_driver.h &&
						cp ../../drivers/popen/source/*.h popen &&
						cp ../../drivers/popen/source/*.c popen &&
						sed -e 's/tcp/gsi/g' tcp/Makefile.am >gsi/Makefile.am &&
						sed -e 's/tcp/gsi/g' tcp/Makefile.in >gsi/Makefile.in &&
						sed -e 's/tcp/popen/g' tcp/Makefile.am >popen/Makefile.am &&
						sed -e 's/tcp/popen/g' tcp/Makefile.in >popen/Makefile.in )

		if(DARWIN)
			set ( GLOBUS_PATCH ${GLOBUS_PATCH} &&
				cd ../../../../../ &&
				patch -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins-2.patch -p0 &&
				patch -i ${CMAKE_CURRENT_SOURCE_DIR}/mac_environ.patch -p0 )
		endif()

		# honestly the build of globus is one of the strangest things I have ever seen and it makes no sense to me.
		ExternalProject_Add(globus
							DEPENDS ${GLOBUS_DEPENDS}
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/globus-5.2.1.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${GLOBUS_STAGE}/dl
							URL http://parrot.cs.wisc.edu/externals/globus-5.2.1.tar.gz
							#--Patch step ----------
							PATCH_COMMAND ${GLOBUS_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND cd gt5.2.1-all-source-installer &&
								ln -s lib ${GLOBUS_INSTALL_LOC}/lib64 &&
							./configure --prefix=${GLOBUS_INSTALL_LOC} --with-flavor=${GLOBUS_FLAVOR}pthr
							#--Build Step ----------
							BUILD_COMMAND cd gt5.2.1-all-source-installer &&
								${GLOBUS_FLAGS} make gpt globus_gssapi_error globus-resource-management-sdk globus-data-management-sdk &&
								cd ${GLOBUS_INSTALL_LOC}/include/globus/ && ln -s ${GLOBUS_FLAVOR}pthr/globus_config.h .
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${GLOBUS_INSTALL_LOC}
							INSTALL_COMMAND touch ${GLOBUS_INSTALL_LOC}/done)

			## Set all the config information
			if ( LINUX )
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.so;${HAVE_LIBLTDL}")
			elseif( DARWIN )
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.dylib")
			else()
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_common.a")
			endif()
			
			# primarily used by cream 
			foreach (_glob_lib ${GLOBUS_FOUND})
				string(REPLACE "${GLOBUS_FLAVOR}" "${GLOBUS_FLAVOR}pthr" _glob_thr_lib ${_glob_lib} )
				append_var(GLOBUS_THR_FOUND "${_glob_thr_lib}")
			endforeach(_glob_lib)

			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_io.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl.a")
			endif()
			
			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol.a")
			endif()

			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control.a")
			endif()

			condor_post_external( globus include/globus OFF )

			if ( LINUX )
				# Install the Globus libraries manually.
				# In order to use an install(FILES) command,
				# we'd have to explicitly list evey file.
				# We can't use a file(GLOB) because that's
				# evaluated when cmake is run, which may be
				# before the external is built.
				install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
				install( CODE "execute_process(COMMAND find ${GLOBUS_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
					COMMAND xargs -isrc cp -a src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
				if ( CONDOR_STRIP_PACKAGES )
					# Strip the libraries manually.
					# Since cmake didn't create the libraries
					# itself, it doesn't know to strip them.
					install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.so*
						COMMAND xargs strip)" )
				endif()
			endif()
			if ( DARWIN )
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

		find_multiple( "globus_gss_assist;globus_gssapi_gsi;globus_gsi_proxy_core;globus_gsi_credential;globus_gsi_callback;globus_gsi_sysconfig;globus_oldgaa;globus_gsi_cert_utils;globus_openssl;globus_openssl_error;globus_proxy_ssl;globus_callout;globus_common;ltdl" GLOBUS_FOUND )
		find_multiple( "globus_gssapi_error;globus_xio;globus_io;globus_rsl" GLOBUS_GRID_UNIVERSE_COMMON )
		find_multiple( "globus_gass_transfer;globus_gram_client;globus_gram_protocol" GLOBUS_GRID_UNIVERSE_GT2 )
		find_multiple( "globus_ftp_control" GLOBUS_GRID_UNIVERSE_NORDUGRID)
		if (GLOBUS_FOUND)
		  append_var (CONDOR_EXTERNAL_INCLUDE_DIRS "/usr/include/globus;/usr/lib64/globus/include;/usr/lib/globus/include;/usr/local/globus/include/globus")
		endif(GLOBUS_FOUND)

	endif(NOT PROPER)

	if (GLOBUS_FOUND)

		message( STATUS "external configured (GLOBUS_FOUND=${GLOBUS_FOUND})" )
		set( HAVE_EXT_GLOBUS ON PARENT_SCOPE )
		set( GLOBUS_FLAVOR ${GLOBUS_FLAVOR} PARENT_SCOPE )
		set( GLOBUS_FOUND ${GLOBUS_FOUND} PARENT_SCOPE )

		if (GLOBUS_GRID_UNIVERSE_COMMON)
			message( STATUS "(GLOBUS_GRID_UNIVERSE_COMMON=${GLOBUS_GRID_UNIVERSE_COMMON})" )
			set( GLOBUS_GRID_UNIVERSE_COMMON ${GLOBUS_GRID_UNIVERSE_COMMON} PARENT_SCOPE )
		endif(GLOBUS_GRID_UNIVERSE_COMMON)

		if (GLOBUS_GRID_UNIVERSE_GT2)
			message( STATUS "(GLOBUS_GRID_UNIVERSE_GT2=${GLOBUS_GRID_UNIVERSE_GT2})" )
			set( GLOBUS_GRID_UNIVERSE_GT2 ${GLOBUS_GRID_UNIVERSE_GT2} PARENT_SCOPE )
		endif(GLOBUS_GRID_UNIVERSE_GT2)

		if (GLOBUS_GRID_UNIVERSE_NORDUGRID)
			message( STATUS "(GLOBUS_GRID_UNIVERSE_NORDUGRID=${GLOBUS_GRID_UNIVERSE_NORDUGRID})" )
			set( GLOBUS_GRID_UNIVERSE_NORDUGRID ${GLOBUS_GRID_UNIVERSE_NORDUGRID} PARENT_SCOPE )
		endif(GLOBUS_GRID_UNIVERSE_NORDUGRID)

	endif(GLOBUS_FOUND)

else( WITH_GLOBUS )

	message (STATUS "external skipped (globus)")

endif( WITH_GLOBUS )
