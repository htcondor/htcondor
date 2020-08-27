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

if (NOT WINDOWS)
	set(transSrcs
td_active.cpp
td_init.cpp
td_main.cpp
td_maint.cpp
td_read_files.cpp
td_utils.cpp
td_write_files.cpp
)

	condor_exe( condor_transferd "${transSrcs}" ${C_SBIN} "${CONDOR_LIBS}" OFF )
endif(NOT WINDOWS)
