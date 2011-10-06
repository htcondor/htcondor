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

file( GLOB DCRmvElements soap_daemon_core* condor_softkill*)

condor_glob( DCHeaderFiles DCSourceFiles "${DCRmvElements};soap_core.cpp;mimetypes.cpp" )

set( DAEMON_CORE_UTIL_SRCS "${DCHeaderFiles};${DCSourceFiles}" PARENT_SCOPE )

if (WINDOWS)
	condor_exe( condor_softkill "condor_softkill.WINDOWS.cpp;condor_softkill.h" ${C_SBIN} "${CONDOR_TOOL_LIBS};psapi" OFF )
	set_target_properties (condor_softkill PROPERTIES WIN32_EXECUTABLE TRUE)
endif(WINDOWS)
