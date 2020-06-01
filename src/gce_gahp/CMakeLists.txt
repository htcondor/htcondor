 ###############################################################
 # 
 # Copyright (C) 1990-2014, Condor Team, Computer Sciences Department,
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

	if ( NOT SQLITE3_LIB )
		message(FATAL_ERROR "gce_gahp requires sqlite3 library")
	endif()
	if ( NOT HAVE_SQLITE3_H )
		message(FATAL_ERROR "gce_gahp requires sqlite3 header")
	endif()

	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	set(SourceFiles
		gceCommands.cpp
		gcegahp_common.cpp
		gce_io_checkfn.cpp
		io_loop_pthread.cpp
		PipeBuffer.cpp
	)

	condor_exe( gce_gahp
		"${SourceFiles}" ${C_SBIN}
		"${CONDOR_TOOL_LIBS};${CURL_FOUND};${SQLITE3_LIB}"
		OFF )

endif()
