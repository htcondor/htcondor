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

if (NOT PROPER AND LINUX)
  option(WITH_GCB "Compiling with support for GCB" ON)
endif()

if (WITH_GCB)

	set (GCB_VER gcb-1.5.6)

	ExternalProject_Add(gcb
				    	PREFIX ${EXTERNAL_BUILD_PREFIX}/${GCB_VER}
				    	#-- Download Step ----------
						DOWNLOAD_COMMAND wget -N http://parrot.cs.wisc.edu/externals/${GCB_VER}.tar.gz ${CMD_TERM}
				    	DOWNLOAD_DIR ${EXTERNAL_DL}
				    	URL http://parrot.cs.wisc.edu/externals/${GCB_VER}.tar.gz
			    		#--Configure step ----------
				    	CONFIGURE_COMMAND cd ${GCB_VER} && ./configure --prefix=${EXTERNAL_STAGE}
				    	#--Build Step ----------
				    	BUILD_COMMAND cd ${GCB_VER} && make
				    	BUILD_IN_SOURCE 1
				    	#--install Step ----------
				    	INSTALL_DIR ${EXTERNAL_STAGE}
				    	INSTALL_COMMAND cd ${GCB_VER} && make install )

	# Set the target dependencies which the rest of condor depends on.
	set(GCB_FOUND "${EXTERNAL_STAGE}/lib/libGCB.a")
	message (STATUS "external configured (GCB_FOUND=${GCB_FOUND})")
	set( GCB_FOUND ${GCB_FOUND} PARENT_SCOPE )
	set( HAVE_EXT_GCB ON PARENT_SCOPE )
	append_var ( CONDOR_EXTERNALS gcb )

	# setup the install target
	install (FILES ${EXTERNAL_STAGE}/bin/gcb_broker_query DESTINATION ${C_SBIN})
	install (FILES ${EXTERNAL_STAGE}/bin/gcb_broker
			${EXTERNAL_STAGE}/bin/gcb_relay_server
			DESTINATION ${C_LIBEXEC} )

else()

	message (STATUS "external skipped (gcb)")

endif()
