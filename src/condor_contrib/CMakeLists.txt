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

##########################################################################
# add the contrib subtree for build & test only

if (WANT_CONTRIB)
  string(REGEX REPLACE "-Werror" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
  #dprint( "CMAKE_CXX_FLAGS = ${CMAKE_CXX_FLAGS}")

  if (HAVE_EXT_POSTGRESQL AND WANT_FULL_DEPLOYMENT)
    option(WANT_QUILL "Support for quill" ON)
  endif()
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}/utils")
  add_subdirectories("${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/utils" "")
else(WANT_CONTRIB)
  dprint("skipping contrib modules")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/campus_factory")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/bosco")
  add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/python-bindings")
endif(WANT_CONTRIB)
