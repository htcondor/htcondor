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

# These are all referenced in condor_utils, we we need absolute paths
set(DAEMON_CORE_UTIL_SRCS_LOCAL
${CMAKE_CURRENT_SOURCE_DIR}/condor_lock_base.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_lock.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_lock_file.cpp
${CMAKE_CURRENT_SOURCE_DIR}/condor_lock_implementation.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_command.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_core.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_core_main.cpp
${CMAKE_CURRENT_SOURCE_DIR}/daemon_keep_alive.cpp
${CMAKE_CURRENT_SOURCE_DIR}/datathread.cpp
${CMAKE_CURRENT_SOURCE_DIR}/HookClient.cpp
${CMAKE_CURRENT_SOURCE_DIR}/HookClientMgr.cpp
${CMAKE_CURRENT_SOURCE_DIR}/self_draining_queue.cpp
${CMAKE_CURRENT_SOURCE_DIR}/self_monitor.cpp
${CMAKE_CURRENT_SOURCE_DIR}/timer_manager.cpp
)

# List APPEND only appends to a local scoped variable
if(WINDOWS)
list(APPEND DAEMON_CORE_UTIL_SRCS_LOCAL
${CMAKE_CURRENT_SOURCE_DIR}/exception_handling.WINDOWS.cpp
${CMAKE_CURRENT_SOURCE_DIR}/pipe.WINDOWS.cpp
)
endif()

# and propogate this variable up to the parent scope
set(DAEMON_CORE_UTIL_SRCS 
	${DAEMON_CORE_UTIL_SRCS_LOCAL}
	PARENT_SCOPE
)

if (WINDOWS)
	condor_exe( condor_softkill "condor_softkill.WINDOWS.cpp;condor_softkill.h" ${C_SBIN} "${CONDOR_TOOL_LIBS};psapi" OFF )
	set_target_properties (condor_softkill PROPERTIES WIN32_EXECUTABLE TRUE)
endif(WINDOWS)
