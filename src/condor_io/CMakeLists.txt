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


file( GLOB CeadarRmvSrcs *.t* test.h )
condor_glob( CedarHdrs CedarSrcs "${CeadarRmvSrcs}" )

set( CEDAR_UTIL_SRCS "${CedarHdrs};${CedarSrcs}" PARENT_SCOPE )

if (NOT WINDOWS)
	condor_exe_test(cedar_test.exe "cedar.t.unix.cpp" "${CONDOR_TOOL_LIBS};${COREDUMPER_FOUND}")
endif()

