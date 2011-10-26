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

		###################################################
		condor_pre_external( GT4 globus-4.0.8 "lib;include" "lib/gt4")
		ExternalProject_Add(gt4
					#-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GT4_VER}-java.tar.gz ${CMD_TERM}
					DOWNLOAD_DIR ${GT4_STAGE}/dl
					URL http://parrot.cs.wisc.edu/externals/${GT4_VER}-java.tar.gz
					#--Configure step ----------
					CONFIGURE_COMMAND echo "Nothing to do"
					#--Build Step ----------
					BUILD_COMMAND echo "Nothing to do"
					BUILD_IN_SOURCE 1
					#--install Step ----------
					INSTALL_DIR ${GT4_INSTALL_LOC}/lib
					INSTALL_COMMAND mkdir -p ${GT4_INSTALL_LOC}/lib/gt4 && cp -r gt4-java/* ${GT4_INSTALL_LOC}/lib/gt4 )
		
		install ( DIRECTORY ${GT4_INSTALL_LOC}/lib/gt4 DESTINATION ${C_LIB} )
		condor_post_external( gt4 OFF OFF )
		###################################################
		condor_pre_external( GT42 globus-4.2.1 "lib;include" "lib/gt42")

		ExternalProject_Add(gt42
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GT42_VER}-java.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${GT42_STAGE}/dl
							URL http://parrot.cs.wisc.edu/externals/${GT42_VER}-java.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND echo "Nothing to do"
							#--Build Step ----------
							BUILD_COMMAND echo "Nothing to do"
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${GT42_INSTALL_LOC}/lib
							INSTALL_COMMAND mkdir -p ${GT42_INSTALL_LOC}/lib/gt42 && cp -r gt42-java/* ${GT42_INSTALL_LOC}/lib/gt42 )

		install ( DIRECTORY ${GT42_INSTALL_LOC}/lib/gt42 DESTINATION ${C_LIB} )
		condor_post_external( gt42 OFF OFF )
		###################################################

		if(${SYS_ARCH} MATCHES "IA64|X86_64")
			set( GLOBUS_FLAVOR gcc64dbg )
		elseif(AIX)
			set( GLOBUS_FLAVOR vendorcc32dbg )
		else ()
			set( GLOBUS_FLAVOR gcc32dbg )
		endif()

		condor_pre_external( GLOBUS globus-5.0.1-p6 "lib;include" "done")

		set (GLOBUS_PATCH patch -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins.patch -p0 &&
						patch -i ${CMAKE_CURRENT_SOURCE_DIR}/rfc_proxy.patch -p0 &&
						cd gt5.0.1-all-source-installer/source-trees/xio/src/builtins &&
						mkdir -p gsi popen &&
						sed -e 's/globus_xio_gsi.h/globus_xio_gsi_driver.h/' ../../drivers/gsi/globus_i_xio_gsi.h >gsi/globus_i_xio_gsi.h &&
						cp ../../drivers/gsi/globus_xio_gsi.c gsi/globus_xio_gsi_driver.c &&
						cp ../../drivers/gsi/globus_xio_gsi.h gsi/globus_xio_gsi_driver.h &&
						cp ../../drivers/popen/source/*.h popen &&
						cp ../../drivers/popen/source/*.c popen &&
						sed -e 's/tcp/gsi/g' tcp/Makefile.am >gsi/Makefile.am &&
						sed -e 's/tcp/gsi/g' tcp/Makefile.in >gsi/Makefile.in &&
						sed -e 's/tcp/popen/g' tcp/Makefile.am >popen/Makefile.am &&
						sed -e 's/tcp/popen/g' tcp/Makefile.in >popen/Makefile.in &&
						cd ../../../../source-trees-thr/xio/src/builtins &&
						mkdir -p gsi popen &&
						cd gsi &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/gsi/Makefile.am . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/gsi/Makefile.in . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/gsi/globus_i_xio_gsi.h . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/gsi/globus_xio_gsi_driver.c . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/gsi/globus_xio_gsi_driver.h . &&
						cd ../popen &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/popen/Makefile.am . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/popen/Makefile.in . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/popen/globus_xio_popen_driver.c . &&
						ln -f -s ../../../../../source-trees/xio/src/builtins/popen/globus_xio_popen_driver.h . )

		if(DARWIN)
			set ( GLOBUS_PATCH ${GLOBUS_PATCH} && cd ../../../../../../ && patch -i ${CMAKE_CURRENT_SOURCE_DIR}/xio-plugins-2.patch -p0 )
		endif()

		#CFLAGS=-I${}/include && export CFLAGS &&
		#LDFLAGS=-L${GLOBUS_INSTALL_LOC}/lib && export LDFLAGS &&
		if ( BUILDING_OPENSSL )
			set (GLOBUS_FLAGS CFLAGS=-I${OPENSSL_INSTALL_LOC}/include && export CFLAGS &&
						LDFLAGS=-L${OPENSSL_INSTALL_LOC}/lib && export LDFLAGS &&
						OPENSSL_CFLAGS=-I${OPENSSL_INSTALL_LOC}/include && export OPENSSL_CFLAGS &&
						OPENSSL_LDFLAGS=-L${OPENSSL_INSTALL_LOC}/lib && export OPENSSL_LDFLAGS  )

			set (MODIFY_LIBTOOL true)
			set (MAKE_GRIDFTP ${GLOBUS_INSTALL_LOC}/sbin/gpt-build -force -static -srcdir=source-trees/gridftp/server/src ${GLOBUS_FLAVOR})

			if(LINUX)
				set (GLOBUS_FLAGS ${GLOBUS_FLAGS} && OPENSSL_LIBS='-lssl -lcrypto -ldl' && export OPENSSL_LIBS)
			elseif(DARWIN)
				set (GLOBUS_FLAGS ${GLOBUS_FLAGS} &&
							OPENSSL_LIBS='-lssl -lcrypto' && export OPENSSL_LIBS &&
							OPENSSL_LDFLAGS='-L${OPENSSL_INSTALL_LOC}/lib -Wl,-search_paths_first' && export OPENSSL_LDFLAGS &&
							LT_MULTI_MODULE=1 && export LT_MULTI_MODULE)
			endif()

			set (GLOBUS_DEPENDS ${OPENSSL_REF} ${GT4_REF} ${GT42_REF})
		else()
			set (GLOBUS_DEPENDS ${GT4_REF} ${GT42_REF})

			if (LINUX)
				set (GLOBUS_FLAGS LDFLAGS='-Wl,--enable-new-dtags' && export LDFLAGS)
				set (MODIFY_LIBTOOL sed -i 's/^hardcode_libdir_flag_spec=.*/hardcode_libdir_flag_spec=/' ${GLOBUS_INSTALL_LOC}/sbin/libtool-*)
				set (MAKE_GRIDFTP LD_RUN_PATH='${EXTERNALS_RPATH}' ${GLOBUS_INSTALL_LOC}/sbin/gpt-build -force -verbose -srcdir=source-trees/gridftp/server/src ${GLOBUS_FLAVOR})
			else()
				set (GLOBUS_FLAGS true)
				set (MODIFY_LIBTOOL true)
				set (MAKE_GRIDFTP ${GLOBUS_INSTALL_LOC}/sbin/gpt-build -force -static -verbose -srcdir=source-trees/gridftp/server/src ${GLOBUS_FLAVOR})
			endif()
		endif()

		# honestly the build of globus is one of the strangest things I have ever seen and it makes no sense to me.
		ExternalProject_Add(globus
							DEPENDS ${GLOBUS_DEPENDS}
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/globus-5.0.1.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${GLOBUS_STAGE}/dl
							URL http://parrot.cs.wisc.edu/externals/globus-5.0.1.tar.gz
							#--Patch step ----------
							PATCH_COMMAND ${GLOBUS_PATCH}
							#--Configure step ----------
							CONFIGURE_COMMAND cd gt5.0.1-all-source-installer &&
								${GLOBUS_FLAGS} &&
							./configure --prefix=${GLOBUS_INSTALL_LOC} --with-flavor=${GLOBUS_FLAVOR}pthr &&
								make gpt gssapi_error globus-resource-management-sdk
							#--Build Step ----------
							BUILD_COMMAND cd gt5.0.1-all-source-installer &&
								${GLOBUS_FLAGS} &&
								./configure --prefix=${GLOBUS_INSTALL_LOC} --with-flavor=${GLOBUS_FLAVOR} &&
								make gpt gssapi_error globus-resource-management-sdk &&
								${MODIFY_LIBTOOL} &&
								GLOBUS_LOCATION=${GLOBUS_INSTALL_LOC} && export GLOBUS_LOCATION &&
								make globus_gridftp_server &&
								${MAKE_GRIDFTP}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${GLOBUS_INSTALL_LOC}
							INSTALL_COMMAND touch ${GLOBUS_INSTALL_LOC}/done)

			## Set all the config information
			if ( LINUX )
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_common_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libltdl_${GLOBUS_FLAVOR}.so")
			elseif( DARWIN )
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_common_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libltdl_${GLOBUS_FLAVOR}.dylib")
			else()
				set (GLOBUS_FOUND "${GLOBUS_INSTALL_LOC}/lib/libglobus_gss_assist_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_gsi_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_proxy_core_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_credential_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_callback_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_sysconfig_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_oldgaa_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gsi_cert_utils_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_openssl_error_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_proxy_ssl_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_callout_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_common_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libltdl_${GLOBUS_FLAVOR}.a")
			endif()
			
			# primarily used by cream 
			foreach (_glob_lib ${GLOBUS_FOUND})
				string(REPLACE "${GLOBUS_FLAVOR}" "${GLOBUS_FLAVOR}pthr" _glob_thr_lib ${_glob_lib} )
				append_var(GLOBUS_THR_FOUND "${_glob_thr_lib}")
			endforeach(_glob_lib)

			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_io_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl_${GLOBUS_FLAVOR}.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_io_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl_${GLOBUS_FLAVOR}.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_COMMON
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gssapi_error_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_xio_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_io_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_rsl_${GLOBUS_FLAVOR}.a")
			endif()
			
			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client_${GLOBUS_FLAVOR}.so;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol_${GLOBUS_FLAVOR}.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client_${GLOBUS_FLAVOR}.dylib;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol_${GLOBUS_FLAVOR}.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_GT2
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_gass_transfer_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_client_${GLOBUS_FLAVOR}.a;${GLOBUS_INSTALL_LOC}/lib/libglobus_gram_protocol_${GLOBUS_FLAVOR}.a")
			endif()

			if ( LINUX )
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control_${GLOBUS_FLAVOR}.so")
			elseif ( DARWIN )
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control_${GLOBUS_FLAVOR}.dylib")
			else()
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID
				"${GLOBUS_INSTALL_LOC}/lib/libglobus_ftp_control_${GLOBUS_FLAVOR}.a")
			endif()
			if ( AIX )
				set (GLOBUS_GRID_UNIVERSE_NORDUGRID "-Wl,-bstatic;${GLOBUS_GRID_UNIVERSE_NORDUGRID};-Wl,-bdynamic")
			endif()

			condor_post_external( globus "include/${GLOBUS_FLAVOR}" OFF )

			install ( PROGRAMS "${GLOBUS_INSTALL_LOC}/sbin/globus-gridftp-server" DESTINATION ${C_LIBEXEC} )
			if ( CONDOR_STRIP_PACKAGES )
				if ( DARWIN )
					install( CODE "execute_process(COMMAND strip -x \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIBEXEC}/globus-gridftp-server)" )
				else()
					install( CODE "execute_process(COMMAND strip \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIBEXEC}/globus-gridftp-server)" )
				endif()
			endif()

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
					install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libltdl*.so*
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
				install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${C_LIBEXEC}/globus-gridftp-server)" )
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.dylib*
					 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libltdl*.dylib*
					 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )

				if ( CONDOR_STRIP_PACKAGES )
					# Strip the libraries manually.
					# Since cmake didn't create the libraries
					# itself, it doesn't know to strip them.
					install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libglobus*.dylib*
						COMMAND xargs strip -x)" )
					install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libltdl*.dylib*
                                                COMMAND xargs strip -x)" )
				endif()
			endif()

	else( NOT PROPER )

		find_multiple( "globus_gss_assist;globus_gssapi_gsi;globus_gsi_proxy_core;globus_gsi_credential;globus_gsi_callback;globus_gsi_sysconfig;globus_oldgaa;globus_gsi_cert_utils;globus_openssl;globus_openssl_error;globus_proxy_ssl;globus_callout;globus_common;ltdl" GLOBUS_FOUND )
		find_multiple( "globus_gssapi_error;globus_xio;globus_io;globus_rsl" GLOBUS_GRID_UNIVERSE_COMMON )
		find_multiple( "globus_gass_transfer;globus_gram_client;globus_gram_protocol" GLOBUS_GRID_UNIVERSE_GT2 )
		find_multiple( "globus_ftp_control" GLOBUS_GRID_UNIVERSE_NORDUGRID)
		if (GLOBUS_FOUND)
		  append_var (CONDOR_EXTERNAL_INCLUDE_DIRS "/usr/include/globus;/usr/lib64/globus/include;/usr/lib/globus/include")
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
