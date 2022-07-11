 ###############################################################
 # 
 # Copyright (C) 1990-2010, Redhat. 
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

if( EXPAT_FOUND AND HAVE_EXT_CURL AND HAVE_PTHREADS )

	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	set(SourceFiles
	amazonCommands.cpp
	amazongahp_common.cpp
	amazon_io_checkfn.cpp
	io_loop_pthread.cpp
	PipeBuffer_no_dc.cpp
	request.cpp
	)

	condor_exe( ec2_gahp
		"${SourceFiles}"
		${C_SBIN}
		"${CONDOR_TOOL_LIBS};${EXPAT_FOUND};${CURL_FOUND}"
		OFF )

    # Build an executable needed by the tests.
	condor_exe_test( queryAPI-sim "queryAPI-sim.cpp" "" )

    set_source_files_properties(queryAPI-sim.cpp PROPERTIES COMPILE_FLAGS -Wno-deprecated)

endif()
