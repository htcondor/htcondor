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

set (DAEMON_CLIENT_UTIL_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/daemon.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_annexd.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_message.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_shadow.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_transfer_queue.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_list.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_types.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_collector.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_master.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_schedd.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_startd.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_starter.cpp
${CMAKE_CURRENT_SOURCE_DIR}/dc_transferd.cpp
PARENT_SCOPE
)
