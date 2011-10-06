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


file( GLOB PrivsepRmvSrcs switchboard* parse_config* safe* )

condor_glob(PrivsepHdrs PrivsepSrcs "${PrivsepRmvSrcs}")

set( PRIVSEP_UTIL_SRCS "${PrivsepHdrs};${PrivsepSrcs}" PARENT_SCOPE )

if (NOT WINDOWS)
	condor_exe( condor_root_switchboard "${PrivsepRmvSrcs};${SAFE_OPEN_SRC}" ${C_SBIN} "" OFF )
endif()
