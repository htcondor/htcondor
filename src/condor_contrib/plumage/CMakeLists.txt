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

if (LINUX)
	option(WITH_PLUMAGE "Support for NoSQL operational data store" ON)
endif()

if ( WITH_PLUMAGE )

 # LTO does not work with BOOST as of GCC 4.7.  Temporarily squashing it.
 STRING (REGEX REPLACE "-flto" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")

 include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/src ${CMAKE_CURRENT_BINARY_DIR}/src ${MONGODB_INCLUDES})
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/src )

endif()
