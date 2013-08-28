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


#postgres is now a contrib only module
if (WANT_CONTRIB AND WANT_FULL_DEPLOYMENT AND (LINUX OR WINDOWS))
  option(WITH_POSTGRESQL "Compiling with support for POSTGRESQL" ON)
endif()

if (WITH_POSTGRESQL)

	if ( NOT PROPER )

		condor_pre_external( POSTGRESQL postgresql-8.2.3 "lib;include" "include/postgres.h")

		if (WINDOWS)
			set (POSTGRESQL_CONFIGURE echo "Nothing to patch on Windows")
			set (POSTGRESQL_MAKE cd src \r\n
			                     nmake -f win32.mak )
			set (POSTGRESQL_INSTALL cp src/interfaces/libpq/Release/*.lib  ${POSTGRESQL_INSTALL_LOC}/lib \r\n
									cp src/interfaces/libpq/Release/*.dll ${POSTGRESQL_INSTALL_LOC}/lib \r\n
									cp src/interfaces/libpq/libpq-fe.h ${POSTGRESQL_INSTALL_LOC}/include \r\n
									cp -r src/include/* ${POSTGRESQL_INSTALL_LOC}/include )
									
									#mt.exe /manifest ${CMAKE_CURRENT_SOURCE_DIR}/libpq.dll.manifest /outputresource:${POSTGRESQL_INSTALL_LOC}/lib/libpq.dll)
									#imbedding custom manifests is a bad idea.
		else()
			set (POSTGRESQL_CONFIGURE ./configure --prefix=${POSTGRESQL_INSTALL_LOC} --without-readline --without-zlib)
			set (POSTGRESQL_MAKE make)
			set (POSTGRESQL_INSTALL make install)
		endif()

		ExternalProject_Add(postgresql
							#-- Download Step ----------
							DOWNLOAD_DIR ${POSTGRESQL_STAGE}/dl
							URL ${EXTERNALS_SOURCE_URL}/${POSTGRESQL_VER}.tar.gz
							#--Configure step ----------
							CONFIGURE_COMMAND ${POSTGRESQL_CONFIGURE}
							#--Build Step ----------
							BUILD_COMMAND ${POSTGRESQL_MAKE}
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${POSTGRESQL_INSTALL_LOC}
							INSTALL_COMMAND ${POSTGRESQL_INSTALL} )

		# Set the target dependencies which the rest of condor depends on.
		if (WINDOWS)
			set(POSTGRESQL_FOUND "${POSTGRESQL_INSTALL_LOC}/lib/libpqdll.lib")

			#this may become the common case.
			install ( FILES ${POSTGRESQL_INSTALL_LOC}/lib/libpq.dll
					  DESTINATION ${C_LIB})
			
		else()
			set(POSTGRESQL_FOUND "${POSTGRESQL_INSTALL_LOC}/lib/libpq.a")
		endif()

		condor_post_external( postgresql include OFF )

	else( NOT PROPER )

		find_multiple( "pq;libpq" POSTGRESQL_FOUND)

	endif( NOT PROPER )

	if (POSTGRESQL_FOUND)
		message (STATUS "external configured (POSTGRESQL_FOUND=${POSTGRESQL_FOUND})")
		set( POSTGRESQL_FOUND ${POSTGRESQL_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_POSTGRESQL ON PARENT_SCOPE )
	endif(POSTGRESQL_FOUND)

else()
	message (STATUS "external skipped (postgresql)")
endif()
