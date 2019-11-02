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


if (HAVE_EXT_GLOBUS)

	condor_exe( gahp_server "gahp_server.cpp;my_ez.cpp" ${C_SBIN}
	"${GLOBUS_GRID_UNIVERSE_GT2};${GLOBUS_GRID_UNIVERSE_COMMON};${GLOBUS_FOUND};${KRB5_FOUND};${OPENSSL_FOUND};${CMAKE_DL_LIBS}" OFF )
	if ( NOT PROPER AND GLOBUS_REF )
		add_dependencies(gahp_server ${GLOBUS_REF})
	endif()

else()

	message(STATUS "NOTE: *gt2* targets will not be built HAVE_EXT_GLOBUS=${HAVE_EXT_GLOBUS}")

endif (HAVE_EXT_GLOBUS)
