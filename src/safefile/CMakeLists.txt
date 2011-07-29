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
if(WINDOWS)
	file(GLOB SAFE_OPEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/safe_open.* ${CMAKE_CURRENT_SOURCE_DIR}/safe_fopen.*)
else()
	file(GLOB SAFE_OPEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/safe_open.* ${CMAKE_CURRENT_SOURCE_DIR}/safe_fopen.* ${CMAKE_CURRENT_SOURCE_DIR}/safe_is_path_trusted.* ${CMAKE_CURRENT_SOURCE_DIR}/safe_id_range_list.c ${CMAKE_CURRENT_BINARY_DIR}/safe_id_range_list.h)
	
#	add_custom_command( OUTPUT config.h safe_id_range_list.h
#						WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
#						COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/configure
#						ARGS --srcdir=${CMAKE_CURRENT_SOURCE_DIR}
#						COMMAND rm
#						ARGS -f ${CMAKE_CURRENT_BINARY_DIR}/config.h
#						)
#	add_custom_target(  configure_safefile
#						ALL
#						DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/safe_id_range_list.h
#						)

	exec_program(${CMAKE_CURRENT_SOURCE_DIR}/configure ${CMAKE_CURRENT_BINARY_DIR}
               ARGS --srcdir=${CMAKE_CURRENT_SOURCE_DIR} && rm -f ${CMAKE_CURRENT_BINARY_DIR}/config.h )

endif()
set(SAFE_OPEN_SRC ${SAFE_OPEN_SRC} PARENT_SCOPE)

message ( STATUS "safe_open done")
message ( STATUS ${SAFE_OPEN_SRC})
message(STATUS ${GSOAP_SRC})
