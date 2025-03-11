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



condor_static_lib( chirp_client "chirp_client.h;chirp_client.c" )
if ( CONDOR_BUILD_SHARED_LIBS )
	condor_shared_lib( chirp_client_shared "chirp_client.h;chirp_client.c" )
	set_target_properties( chirp_client_shared PROPERTIES OUTPUT_NAME chirp_client )
endif()

if (WINDOWS)
	target_link_libraries(chirp_client ${CONDOR_WIN_LIBS})    
endif(WINDOWS)

condor_exe( condor_chirp "condor_chirp.cpp;${SAFE_OPEN_SRC}" ${C_LIBEXEC}
"chirp_client" OFF )

install ( FILES Chirp.jar DESTINATION ${C_LIB} )

install ( FILES chirp_client.h DESTINATION ${C_INCLUDE})
if (NOT WINDOWS)
    install (TARGETS chirp_client DESTINATION ${C_LIB})
endif(NOT WINDOWS)
