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

# These will be expanded by condor_utils, so we need absolute paths
set(CCB_UTIL_SRCS
${CMAKE_CURRENT_SOURCE_DIR}/ccb_client.cpp
${CMAKE_CURRENT_SOURCE_DIR}/ccb_listener.cpp
${CMAKE_CURRENT_SOURCE_DIR}/ccb_server.cpp
PARENT_SCOPE
)
