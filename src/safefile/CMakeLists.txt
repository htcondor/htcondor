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
	set(SAFE_OPEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/safe_open.c ${CMAKE_CURRENT_SOURCE_DIR}/safe_fopen.c)
else()
	set(SAFE_OPEN_SRC ${CMAKE_CURRENT_SOURCE_DIR}/safe_open.c ${CMAKE_CURRENT_SOURCE_DIR}/safe_fopen.c ${CMAKE_CURRENT_SOURCE_DIR}/safe_is_path_trusted.c ${CMAKE_CURRENT_SOURCE_DIR}/safe_id_range_list.c)
endif()

set(SAFE_OPEN_SRC ${SAFE_OPEN_SRC} PARENT_SCOPE)

message ( STATUS "safe_open done")
message ( STATUS ${SAFE_OPEN_SRC})
