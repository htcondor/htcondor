 ###############################################################
 # 
 # Copyright 2014 University of Wisconsin-Madison
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

if (WINDOWS)
	set(AUTH_SERVER_SRCS "" PARENT_SCOPE)
else()
	file(GLOB ASRmvElements auth_server_main.cpp auth_server.*)
	condor_glob(ASHeaderFiles ASSourceFiles "${ASRmvElements}")
	set(AUTH_SERVER_SRCS "${ASHeaderFiles};${ASSourceFiles}" PARENT_SCOPE)
	condor_exe(condor_auth_server "auth_server_main.cpp;auth_server.cpp" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF)
endif()
