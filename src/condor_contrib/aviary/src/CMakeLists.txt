 ###############################################################
 # 
 # Copyright (C) 2009-2011, Red Hat, Inc.
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

if (LINUX)

    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
    file( GLOB RmvSrcs test* *_main* )
    condor_glob( Hdrs Srcs "${RmvSrcs}" )

    set( WS02_GEN_SRCS "TBD")

    # ws02 gen & copy out the results for out of source builds
    add_custom_command( OUTPUT ${WS02_GEN_SRCS} 
                        COMMAND wsdl2cpp
                        ARGS ${WS02_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/aviary-query.wsdl)

    condor_static_lib( aviaryquery "${Hdrs};${Srcs};${WS02_GEN_SRCS}" ) # just to build all srcs then let the linker sort it out.
    condor_exe( aviary_query_server "job_server_main.cpp" "${C_SBIN}" "jobserver;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND}" OFF )

endif(LINUX)
