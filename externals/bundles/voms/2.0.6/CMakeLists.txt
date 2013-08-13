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

# The source tarball voms-2.0.6.tar.gz was taken from the git
# repository git://testbed002.cnaf.infn.it/opt/gits/voms.git using
# tag voms_R_2_0_6.

if (LINUX OR DARWIN)
  option( WITH_VOMS "Compiling with support for VOMS" ON )
endif(LINUX OR DARWIN)

if ( WITH_VOMS )

	if (NOT PROPER)

		if (NOT EXPAT_FOUND)
			message(FATAL_ERROR "Can't find expat, needed to build voms external!")
		endif()

		if ( DARWIN )
			set ( VOMS_PATCH patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/voms-gsoap.patch &&
				rm src/server/stdsoap2.c src/server/stdsoap2.h src/server/soapC.c src/server/soapH.h src/server/soapStub.h src/server/soapdefs.h &&
				touch -r src/utils/vomsfake.y src/utils/vomsparser.h &&
				touch -r src/utils/vomsfake.y src/utils/vomsparser.c &&
				touch -r src/utils/vomsfake.y src/utils/lex.yy.c &&
				patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/globus_thread_h.patch &&
				patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/new_cpp_linkage.patch &&
				cp ${CMAKE_CURRENT_SOURCE_DIR}/dummy.c src/replib &&
				patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/replib.patch )
			set ( VOMS_GLOBUS_FLAG "" )
		ELSE()
			set ( VOMS_PATCH patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/voms-gsoap.patch &&
				rm src/server/stdsoap2.c src/server/stdsoap2.h src/server/soapC.c src/server/soapH.h src/server/soapStub.h src/server/soapdefs.h &&
				touch -r src/utils/vomsfake.y src/utils/vomsparser.h &&
				touch -r src/utils/vomsfake.y src/utils/vomsparser.c &&
				touch -r src/utils/vomsfake.y src/utils/lex.yy.c &&
				patch -N -p1 < ${CMAKE_CURRENT_SOURCE_DIR}/globus_thread_h.patch &&
				./autogen.sh )
			set ( VOMS_GLOBUS_FLAG --with-globus-prefix=${GLOBUS_INSTALL_LOC} )
		ENDIF()

		condor_pre_external( VOMS voms-2.0.6 "include;lib" "done")

		if ( ${OS_NAME} STREQUAL "DARWIN" AND "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
			string(REPLACE "-std=c++11" "" VOMS_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
			string(REPLACE "-std=c++11" "" VOMS_C_FLAGS "${CMAKE_C_FLAGS}")
			string(REPLACE "-std=c++11" "" VOMS_LDFLAGS "${CMAKE_SHARED_LINKER_FLAGS}")
			set ( VOMS_CXX_COMPILER  "clang++" )
                else()
			set ( VOMS_CXX_FLAGS ${CMAKE_CXX_FLAGS} )
			set ( VOMS_C_FLAGS ${CMAKE_C_FLAGS} )
			set ( VOMS_LDFLAGS ${CMAKE_SHARED_LINKER_FLAGS} )
			set ( VOMS_CXX_COMPILER ${CMAKE_CXX_COMPILER} )
		endif()

		ExternalProject_Add(voms
			DEPENDS ${GLOBUS_REF}
			#-- Download Step ----------
			DOWNLOAD_DIR ${VOMS_STAGE}/dl
			URL ${EXTERNALS_SOURCE_URL}/voms-2.0.6.tar.gz
			#--Patch step ----------
			PATCH_COMMAND ${VOMS_PATCH}
			#--Configure step ----------
			CONFIGURE_COMMAND 
				export CXX=${CMAKE_CXX_COMPILER} &&
				export CC=${CMAKE_C_COMPILER} &&
				export CXXFLAGS=${VOMS_CXX_FLAGS} &&
				export CFLAGS=${VOMS_C_FLAGS} &&
				export LDFLAGS=${VOMS_LDFLAGS} &&
				./configure
					--prefix=${VOMS_INSTALL_LOC}
					--with-api-only
					${VOMS_GLOBUS_FLAG}
					--disable-java --disable-docs
					--disable-glite
			#--Build Step ----------
			BUILD_COMMAND make
			BUILD_IN_SOURCE 1
			#--install Step ----------
			INSTALL_DIR ${VOMS_INSTALL_LOC}
			INSTALL_COMMAND make install && touch ${VOMS_INSTALL_LOC}/done
		)

		set( VOMS_FOUND_STATIC "${VOMS_INSTALL_LOC}/lib/libvomsapi.a" )
		if ( LINUX )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.so" )
		elseif ( DARWIN )
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.dylib" )
		else()
			set( VOMS_FOUND "${VOMS_INSTALL_LOC}/lib/libvomsapi.a" )
		endif()

		condor_post_external( voms include OFF )

		if ( LINUX )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/lib -maxdepth 1 -name *.so*
				COMMAND xargs -isrc cp -a src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.so*
					COMMAND xargs strip)" )
			endif()
			# Check libvomsapi's shared library dependencies
			# and copy a specific subset of those libraries
			# into <release>/lib/condor
			# This is to pick up libexpat, which libvomsapi
			# depends on, but none of the binaries that use it do.
			if ( NOT CONDOR_PACKAGE_BUILD )
				set( SRC_DIR ${CMAKE_SOURCE_DIR} )
				install( CODE "execute_process(COMMAND mkdir -p ${CMAKE_INSTALL_PREFIX}/lib/condor)" )
				install( CODE "execute_process(COMMAND ${SRC_DIR}/src/condor_scripts/print-libs ${VOMS_INSTALL_LOC}/lib/libvomsapi.so ${SRC_DIR}/src/condor_scripts/syslib-patterns
					 COMMAND xargs -isrc cp src ${CMAKE_INSTALL_PREFIX}/lib/condor
			 		 )" )
			endif()
		endif()
		if ( DARWIN )
			# Install the VOMS libraries manually.
			# In order to use an install(FILES) command, we'd
			# have to explicitly list evey file.
			# We can't use a file(GLOB) because that's evaluated
			# when cmake is run, which may be before the
			# external is built.
			install( CODE "execute_process(COMMAND mkdir -p \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )
			install( CODE "execute_process(COMMAND find ${VOMS_INSTALL_LOC}/lib -maxdepth 1 -name *.dylib*
				COMMAND xargs -I src cp -pPR src \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB})" )

				# Fix up the share library dependencies
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.dylib*
					 COMMAND xargs ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs)" )

			if ( CONDOR_STRIP_PACKAGES )
				# Strip the libraries manually.
				# Since cmake didn't create the libraries
				# itself, it doesn't know to strip them.
				install( CODE "execute_process(COMMAND find \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${EXTERNALS_LIB} -name libvoms*.dylib*
					COMMAND xargs strip -x)" )
			endif()
		endif()

	else(NOT PROPER)

		find_multiple( "vomsapi" VOMS_FOUND)

	endif(NOT PROPER)

	if (VOMS_FOUND)
		message ( STATUS "external configured (VOMS_FOUND=${VOMS_FOUND})" )
		set( HAVE_EXT_VOMS ON PARENT_SCOPE )
		set( VOMS_FOUND ${VOMS_FOUND} PARENT_SCOPE )
		set( VOMS_FOUND_STATIC ${VOMS_FOUND_STATIC} PARENT_SCOPE )
	endif()

endif()

if(NOT VOMS_FOUND)
	message ( STATUS "external skipped (voms)" )
endif()
