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

if (HAVE_EXT_OPENSSL AND HAVE_EXT_CURL AND HAVE_EXT_PCRE AND HAVE_PTHREADS)

	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

    condor_glob( HeaderFiles SourceFiles "queryAPI-sim.cpp" )

	condor_exe( ec2_gahp
    	"${HeaderFiles};${SourceFiles}"	${C_SBIN}
	    "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${CURL_FOUND}"
    	OFF)

	# condor_exe_test(ec2_sim
	#    "queryAPI-sim.cpp"
	#    "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND}")

endif()
