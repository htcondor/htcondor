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


include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(qmgmtElements
	${CMAKE_CURRENT_SOURCE_DIR}/qmgmt_send_stubs.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/qmgr_lib_support.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/qmgr_job_updater.cpp
)

file( GLOB scheddRmvElements Example* *.t.* ${qmgmtElements} )

condor_daemon( schedd "${scheddRmvElements}"
  "${CONDOR_LIBS};${CONDOR_QMF};${ZLIB_FOUND}" "${C_SBIN}")

set( QMGMT_UTIL_SRCS "${qmgmtElements};${CMAKE_CURRENT_SOURCE_DIR}/qmgmt_common.cpp" PARENT_SCOPE )
