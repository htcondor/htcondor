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
  option( WITH_VOMS "Compiling with support for VOMS" ON )
endif(LINUX OR DARWIN)

if ( WITH_VOMS )

	if (NOT PROPER)

		set (VOMS_VER voms-1.9.10_4)

		ExternalProject_Add(voms
							DEPENDS expat openssl globus zlib
							PREFIX ${EXTERNAL_BUILD_PREFIX}/${VOMS_VER}
							#-- Download Step ----------
							DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${VOMS_VER}.tar.gz ${CMD_TERM}
							DOWNLOAD_DIR ${EXTERNAL_DL}
							URL http://parrot.cs.wisc.edu/externals/${VOMS_VER}.tar.gz
							#--Patch step ----------
							PATCH_COMMAND patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/configure.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/makefile.patch &&
								patch -p0 < ${CMAKE_CURRENT_SOURCE_DIR}/private.patch &&
								${CMAKE_CURRENT_SOURCE_DIR}/voms_file_patch.sh
							#--Configure step ----------
							CONFIGURE_COMMAND cd org.glite.security.voms &&
								./configure
								--prefix=${EXTERNAL_STAGE}
								--with-api-only
								--with-openssl-prefix=${EXTERNAL_STAGE}
								--with-expat-prefix=${EXTERNAL_STAGE}
								--with-globus-prefix=${EXTERNAL_STAGE}
								--with-globus-flavor=${GLOBUS_FLAVOR} --disable-java --disable-docs --disable-glite
							#--Build Step ----------
							BUILD_COMMAND cd org.glite.security.voms && make
							BUILD_IN_SOURCE 1
							#--install Step ----------
							INSTALL_DIR ${EXTERNAL_STAGE}
							INSTALL_COMMAND cd org.glite.security.voms && make install
				)

		# On Mac OS X, giving the basename of the library causes
		# it to be dynamically linked. On linux, the library may
		# be in lib/ or lib64/.
		if ( DARWIN )
			set( VOMS_FOUND "${EXTERNAL_STAGE}/lib/libvomsapi_${GLOBUS_FLAVOR}.a" )
		else()
			set( VOMS_FOUND "libvomsapi_${GLOBUS_FLAVOR}.a" )
		endif()

	else(NOT PROPER)

		find_multiple( "vomsapi" VOMS_FOUND)

	endif(NOT PROPER)

	if (VOMS_FOUND)
		message ( STATUS "external configured (VOMS_FOUND=${VOMS_FOUND})" )
		set( HAVE_EXT_VOMS ON PARENT_SCOPE )
		set( VOMS_FOUND ${VOMS_FOUND} PARENT_SCOPE )
		append_var(CONDOR_EXTERNALS voms)
		append_var(EXTERNAL_INCLUDES ${EXTERNAL_STAGE}/include/glite/security)
	endif()

endif()

if(NOT VOMS_FOUND)
	message ( STATUS "external skipped (voms)" )
endif()
