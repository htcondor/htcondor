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
option(WITH_MANAGEMENT "Support for qmf plugin" ON)

if (WITH_MANAGEMENT AND HAVE_EXT_QPID AND WANT_QMF)
 
 include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/qmf/plugins ${CMAKE_CURRENT_BINARY_DIR}/qmf/plugins )
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/qmf/plugins )
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/qmf/daemons )
 add_subdirectory( ${CMAKE_CURRENT_SOURCE_DIR}/qmf/etc )

endif()
