 ###############################################################
 # 
 # Copyright (C) 1990-2021, Condor Team, Computer Sciences Department,
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

if( NOT WINDOWS )

	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	set(SourceFiles
		arcCommands.cpp
		arcgahp_common.cpp
		io_loop_pthread.cpp
		PipeBuffer.cpp
	)

	condor_exe( arc_gahp
		"${SourceFiles}" ${C_SBIN}
		"${CONDOR_TOOL_LIBS};${CURL_FOUND}"
		OFF )

endif()
