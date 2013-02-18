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

add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/common")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/job")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/query")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/locator")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/collector")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/hadoop")