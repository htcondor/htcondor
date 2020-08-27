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

set(CGahpSrcs
	PipeBuffer.cpp
	io_loop.cpp
	cgahp_common.cpp
)
 
condor_exe( condor_c-gahp "${CGahpSrcs}" ${C_SBIN} "${CONDOR_LIBS}" OFF )

set(CGahpWorkerSrcs
	PipeBuffer.cpp
	cgahp_common.cpp
	schedd_client.cpp
	schedd_client_main.cpp
	SchedDCommands.cpp
)

condor_exe( condor_c-gahp_worker_thread "${CGahpWorkerSrcs}" ${C_SBIN} "${CONDOR_LIBS}" OFF )
