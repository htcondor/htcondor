 ###############################################################
 # 
 # Copyright (C) 2022, Condor Team, Computer Sciences Department,
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

if(UNIX)
	condor_exe(rvgahp_server "rvgahp_server.c;common.c" ${C_SBIN} "" OFF)
	set_target_properties( rvgahp_server PROPERTIES COMPILE_FLAGS "-Wno-unused-result")
	condor_exe(rvgahp_proxy "rvgahp_proxy.c;common.c" ${C_SBIN} "" OFF)
	condor_exe(rvgahp_client "rvgahp_client.c;common.c" ${C_SBIN} "" OFF)
endif()
