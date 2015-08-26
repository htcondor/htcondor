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
include_directories(${CONDOR_SCHEDD_SRC_DIR})

file( GLOB HADOOP_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/hadoop/src/*.cpp )
file( GLOB HADOOP_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryHadoopService*.cpp )

file( GLOB HADOOP_PLUGIN_SRCS ../common/ClassadCodec.cpp ../common/AviaryUtils.cpp HadoopObject.cpp AviaryHadoopPlugin*.cpp )

############################
# hadoop plugin
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

add_library (aviary_hadoop_axis SHARED ${HADOOP_GEN_SRCS} ${HADOOP_SRCS} ../common/ClassadCodec.cpp ../common/AviaryUtils.cpp HadoopObject.cpp )
# remove unresolved deps by copying in -lwso2_wsf objects
target_link_libraries(aviary_hadoop_axis -Wl,--whole-archive -lwso2_wsf -Wl,--no-whole-archive)
target_link_libraries(aviary_hadoop_axis aviary_wso2_common)

condor_plugin( AviaryHadoopPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${HADOOP_PLUGIN_SRCS}" "${C_LIBEXEC}" "${WSO2_FOUND}" ON)
# remove unresolved deps by copying in -laxis2_engine objects
target_link_libraries(AviaryHadoopPlugin-plugin -Wl,--whole-archive -laxis2_engine -Wl,--no-whole-archive)
# link externally to provider and common
target_link_libraries(AviaryHadoopPlugin-plugin aviary_axis_provider)
target_link_libraries(AviaryHadoopPlugin-plugin aviary_wso2_common)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/63aviary-hadoop.config DESTINATION "${C_ETC_EXAMPLES}")

install (TARGETS aviary_hadoop_axis DESTINATION ${C_SERVICES}/hadoop)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../services/hadoop/services.xml DESTINATION ${C_SERVICES}/hadoop )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-common.xsd
                ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-hadoop.xsd
                ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-hadoop.wsdl
        DESTINATION ${C_SERVICES}/hadoop )
