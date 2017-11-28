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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

############################
# file target references.
############################
file( GLOB QUERY_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/query/src/*.cpp )
file( GLOB QUERY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryQueryService*.cpp )

file( GLOB JOB_SERVER_SRCS aviary_query_server.cpp 
            SubmissionObject.cpp JobServerJobLogConsumer.cpp
            Job.cpp HistoryFile.cpp HistoryProcessingUtils.cpp Globals.cpp
            JobServerObject.cpp ../common/ClassadCodec.cpp  ../common/AviaryUtils.cpp)

############################
# query server
############################
add_library (aviary_query_axis SHARED ${QUERY_GEN_SRCS} ${QUERY_SRCS} )
target_link_libraries (aviary_query_axis ${WSO2_FOUND})

condor_exe( aviary_query_server "${JOB_SERVER_SRCS}" "${C_SBIN}" 
    "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${WSO2_FOUND};aviary_wso2_common;aviary_axis_provider" OFF )

############################
# install target
############################
install (TARGETS aviary_query_axis DESTINATION ${C_SERVICES}/query)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../services/query/services.xml DESTINATION ${C_SERVICES}/query )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-common.xsd
            ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-query.xsd
            ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-query.wsdl
            DESTINATION ${C_SERVICES}/query )
