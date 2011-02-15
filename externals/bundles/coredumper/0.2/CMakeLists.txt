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


if (LINUX AND ${SYS_ARCH} MATCHES "I386|X86_64")
  option(WITH_COREDUMPER "Compiling with support for COREDUMPER" ON)
endif()

if (WITH_COREDUMPER)

	if ( NOT PROPER )

		set (CRDPR_VER coredumper-0.2)

		ExternalProject_Add(coredumper
				    PREFIX ${EXTERNAL_BUILD_PREFIX}/${CRDPR_VER}
				    #-- Download Step ----------
					DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${CRDPR_VER}.tar.gz ${CMD_TERM}
				    DOWNLOAD_DIR ${EXTERNAL_DL}
				    URL http://parrot.cs.wisc.edu/externals/${CRDPR_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND cd ${CRDPR_VER} && ./configure --prefix=${EXTERNAL_STAGE}
				    #--Build Step ----------
				    BUILD_COMMAND cd ${CRDPR_VER} && make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${EXTERNAL_STAGE}
				    INSTALL_COMMAND cd ${CRDPR_VER} && make install )

		# Set the target dependencies which the rest of condor depends on.
		set(COREDUMPER_FOUND "${EXTERNAL_STAGE}/lib/libcoredumper.a")
		append_var (CONDOR_EXTERNALS coredumper)

	else( NOT PROPER )

		find_multiple( "coredumper" COREDUMPER_FOUND )

	endif( NOT PROPER )

	if (COREDUMPER_FOUND)
		message (STATUS "external configured (COREDUMPER_FOUND=${COREDUMPER_FOUND})")
		set( COREDUMPER_FOUND ${COREDUMPER_FOUND} PARENT_SCOPE )
		set( HAVE_EXT_COREDUMPER ON PARENT_SCOPE )
	endif(COREDUMPER_FOUND)

else(WITH_COREDUMPER)

	message (STATUS "external skipped (coredumper)")

endif(WITH_COREDUMPER)
