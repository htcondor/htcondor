 ###############################################################
 # 
 # Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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

dprint("PIGEON does not build when enabled!!")
option(WITH_PIGEON "Support for condor_pigeon" OFF)

if ( WITH_PIGEON AND HAVE_EXT_QPID )
 
 include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/src/condor_pigeon/ ${CMAKE_CURRENT_SOURCE_DIR}/src/condor_pigeon_client ${CMAKE_CURRENT_BINARY_DIR}/src/condor_pigeon/ )
 
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/src/condor_pigeon/ )
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/src/condor_pigeon_client/ )

endif()
