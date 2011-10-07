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


condor_selective_glob("server_interface.*" ServerAPISrcs )

set( CKPT_SERVER_UTIL_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/network2.cpp;${CMAKE_CURRENT_SOURCE_DIR}/network2.h;${ServerAPISrcs}" PARENT_SCOPE )


if(STD_UNIVERSE)
	condor_selective_glob("server_file*" unusedSrcs )
	condor_glob( HeaderFiles SourceFiles "${ServerAPISrcs};${unusedSrcs}" )
	condor_exe( condor_ckpt_server "${HeaderFiles};${SourceFiles}" ${C_SBIN} "${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND};${PCRE_FOUND}" OFF )
endif(STD_UNIVERSE)
