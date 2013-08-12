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


if (LINUX AND ${SYS_ARCH} MATCHES "X86_64|I686|I386")
  option(WITH_COREDUMPER "Compiling with support for COREDUMPER" ON)
endif()

if (WITH_COREDUMPER)

	if ( NOT PROPER )

		condor_pre_external( COREDUMPER coredumper-0.2 "lib;include" "done")

		ExternalProject_Add(coredumper
				    #-- Download Step ----------
				    DOWNLOAD_DIR ${COREDUMPER_STAGE}/dl
				    URL ${EXTERNALS_SOURCE_URL}/${COREDUMPER_VER}.tar.gz
				    #--Configure step ----------
				    CONFIGURE_COMMAND ./configure --prefix=${COREDUMPER_INSTALL_LOC}
				    #--Build Step ----------
				    BUILD_COMMAND make
				    BUILD_IN_SOURCE 1
				    #--install Step ----------
				    INSTALL_DIR ${COREDUMPER_INSTALL_LOC}
				    INSTALL_COMMAND make install && touch ${COREDUMPER_INSTALL_LOC}/done)

		# Set the target dependencies which the rest of condor depends on.
		set(COREDUMPER_FOUND "${COREDUMPER_INSTALL_LOC}/lib/libcoredumper.a")

		condor_post_external( coredumper include OFF )

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
