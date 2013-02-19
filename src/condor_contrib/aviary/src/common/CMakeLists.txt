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

# shouldn't happen if gen code checked-in
if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../codegen" )
     message( FATAL_ERROR "run ../../gen-xsd-cpp.sh first!")
endif ()

############################
# file target references.
############################
file( GLOB SOAP_SRCS ${CMAKE_CURRENT_SOURCE_DIR} EndpointPublisher.cpp AviaryProviderFactory.cpp Axis2SoapProvider.cpp)
file( GLOB COMMON_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/common/src/*.cpp )

if (WITH_OPENSSL)
    file( GLOB SSL_SRCS ${CMAKE_CURRENT_SOURCE_DIR} axis2_ssl_*.c Axis2SslProvider.cpp)
    list(APPEND SOAP_SRCS ${SSL_SRCS} )
endif(WITH_OPENSSL)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

# link provider as shared lib
add_library (aviary_axis_provider SHARED ${SOAP_SRCS})
add_library (aviary_wso2_common SHARED ${COMMON_GEN_SRCS})

############################
# install targets && configure our axis2.xml for platform
############################
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../../etc/axis2.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/../../etc/axis2.xml" @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/../../etc/axis2.xml DESTINATION ${CMAKE_INSTALL_PREFIX} )

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/61aviary.config DESTINATION "${C_ETC_EXAMPLES}")
file(GLOB TEST_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.py ${CMAKE_CURRENT_SOURCE_DIR}/../../test/*.sh)
file(GLOB DAG_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../test/dag/*)
file(GLOB SUPPORT_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../../test/module/aviary/*)
install(FILES ${TEST_SCRIPTS} DESTINATION ${C_LIB}/aviary PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${DAG_SCRIPTS} DESTINATION ${C_LIB}/aviary/dag PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${SUPPORT_SCRIPTS} DESTINATION ${C_LIB}/aviary/module/aviary PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../README DESTINATION ${C_LIB}/aviary)

install (TARGETS aviary_axis_provider DESTINATION ${C_LIB})
install (TARGETS aviary_wso2_common DESTINATION ${C_LIB})