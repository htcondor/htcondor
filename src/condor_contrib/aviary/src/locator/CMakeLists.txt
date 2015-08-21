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
include_directories(${CONDOR_COLLECTOR_SRC_DIR})

############################
# file target references.
############################
file( GLOB LOCATOR_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/locator/src/*.cpp )
file( GLOB LOCATOR_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryLocatorService*.cpp )

file( GLOB LOCATOR_PLUGIN_SRCS ../common/AviaryUtils.cpp LocatorObject.cpp 
            AviaryLocatorPlugin*.cpp )

############################
# locator plugin
############################
condor_plugin( AviaryLocatorPlugin-plugin "${COLLECTOR_REF};${LOCATOR_PLUGIN_SRCS}" "${C_LIBEXEC}" "${WSO2_FOUND}" ON)
# remove unresolved deps by copying in -laxis2_engine objects
target_link_libraries(AviaryLocatorPlugin-plugin -Wl,--whole-archive -laxis2_engine -Wl,--no-whole-archive)

add_library (aviary_locator_axis SHARED ${LOCATOR_GEN_SRCS} ${LOCATOR_SRCS})
# remove unresolved deps by copying in -lwso2_wsf objects
target_link_libraries(aviary_locator_axis -Wl,--whole-archive -lwso2_wsf -Wl,--no-whole-archive)
# axis2 engine linking forcing our hand again
target_link_libraries(aviary_locator_axis AviaryLocatorPlugin-plugin)
# link externally to provider and common
target_link_libraries(AviaryLocatorPlugin-plugin aviary_axis_provider)
target_link_libraries(AviaryLocatorPlugin-plugin aviary_wso2_common)

############################
# install target
############################
install (TARGETS aviary_locator_axis DESTINATION ${C_SERVICES}/locator)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../services/locator/services.xml DESTINATION ${C_SERVICES}/locator )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-common.xsd
				${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-locator.xsd
				${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-locator.wsdl
		DESTINATION ${C_SERVICES}/locator )
