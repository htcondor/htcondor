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
    condor_glob( Hdrs Srcs "${RmvSrcs}" )

    set ( WS02_GEN_SRCS AviaryQueryService.cpp )
    dprint( "WS02_GEN_SRCS = ${WS02_GEN_SRCS}" )

    add_custom_command( TARGET aviaryquery
                        PRE_BUILD
                        COMMAND WSDL2CPP.sh
                        ARGS -u ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.wsdl -o ../codegen/query -d adb )

    file( GLOB_RECURSE WS02_GEN_SRCS codegen/query/*.cpp )

    dprint( "RmvSrcs = ${RmvSrcs}" )
    dprint( "Srcs = ${Srcs}" )
    dprint( "Hdrs = ${Hdrs}" )
    dprint( "WS02_GEN_SRCS = ${WS02_GEN_SRCS}" )

    condor_static_lib( aviaryquery "${Hdrs};${Srcs};${WS02_GEN_SRCS}" )
    condor_exe( aviary_query_server "aviary_query_server.cpp" "${C_SBIN}" "aviaryquery;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND}" OFF )

endif(LINUX)
