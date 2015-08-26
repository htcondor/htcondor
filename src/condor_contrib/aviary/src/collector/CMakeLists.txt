 ###############################################################
 # 
 # Copyright 2013 Red Hat, Inc. 
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
include_directories(${CONDOR_COLLECTOR_SRC_DIR})

############################
# file target references.
############################
file( GLOB COLLECTOR_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/collector/src/*.cpp )
file( GLOB COLLECTOR_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryCollectorService*.cpp )

file( GLOB COLLECTOR_PLUGIN_SRCS ../common/AviaryUtils.cpp ../common/ClassadCodec.cpp Collectables.cpp CollectorObject.cpp CollectableCodec.cpp 
        AviaryCollectorPlugin*.cpp )

############################
# schedd plugin
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

############################
# collector plugin
############################
condor_plugin( AviaryCollectorPlugin-plugin "${COLLECTOR_REF};${COMMON_GEN_SRCS};${COLLECTOR_PLUGIN_SRCS}" "${C_LIBEXEC}" "${WSO2_FOUND}" ON)
# remove unresolved deps by copying in -laxis2_engine objects
target_link_libraries(AviaryCollectorPlugin-plugin -Wl,--whole-archive -laxis2_engine -Wl,--no-whole-archive)

add_library (aviary_collector_axis SHARED ${COLLECTOR_GEN_SRCS} ${COLLECTOR_SRCS})
# remove unresolved deps by copying in -lwso2_wsf objects
target_link_libraries(aviary_collector_axis -Wl,--whole-archive -lwso2_wsf -Wl,--no-whole-archive)
# axis2 engine linking forcing our hand again
target_link_libraries(aviary_collector_axis AviaryCollectorPlugin-plugin)
# link externally to provider and common
target_link_libraries(AviaryCollectorPlugin-plugin aviary_axis_provider)
target_link_libraries(AviaryCollectorPlugin-plugin aviary_wso2_common)

############################
# install targets && configure our axis2.xml for platform
############################
install (TARGETS aviary_collector_axis DESTINATION ${C_SERVICES}/collector)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../services/collector/services.xml DESTINATION ${C_SERVICES}/collector )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-common.xsd
        ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-collector.xsd
        ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-collector.wsdl
        DESTINATION ${C_SERVICES}/collector )
