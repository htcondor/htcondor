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


file( GLOB ProcdRmvElements *_test* proc_family_client* proc_family_io*
local_client.UNIX* procd_ctl* gidd_alloc*)

if (WINDOWS)
	set (PROCD_WIN_LINK_LIBS psapi)
	condor_selective_glob ( "proc_family_client*;proc_family_io*;local_client.*" ProcClientElements)
else(WINDOWS)
    condor_selective_glob( "proc_family_client*;proc_family_io*;local_client.*;named_pipe_reader*;named_pipe_writer*;named_pipe_util*;named_pipe_watchdog*" ProcClientElements )
endif(WINDOWS)

set( ProcdUtilsSrcs "${SAFE_OPEN_SRC};../condor_utils/condor_pidenvid.cpp;../condor_utils/condor_full_io.cpp;../condor_utils/condor_blkng_full_disk_io.cpp;../condor_utils/selector.cpp;../condor_procapi/procapi.cpp;../condor_procapi/processid.cpp;../condor_procapi/procapi_killfamily.cpp;../condor_starter.V6.1/cgroup.linux.cpp" )
if (WINDOWS)
	set( ProcdUtilsSrcs "${ProcdUtilsSrcs};../condor_utils/process_control.WINDOWS.cpp;../condor_utils/ntsysinfo.WINDOWS.cpp" )
endif(WINDOWS)
condor_static_lib( procdutils "${ProcdUtilsSrcs}" )
condor_daemon(procd "${ProcdRmvElements}" "procdutils;${PROCD_WIN_LINK_LIBS};${LIBCGROUP_FOUND}" "${C_SBIN}" OFF)

if (LINUX AND WANT_FULL_DEPLOYMENT)
	condor_exe( procd_ctl "procd_ctl.cpp;${ProcClientElements};${SAFE_OPEN_SRC};../condor_utils/distribution.cpp;../condor_utils/my_distribution.cpp;../condor_utils/condor_pidenvid.cpp;dprintf_lite.cpp" ${C_SBIN} "procdutils" OFF)

	condor_exe( gidd_alloc "gidd_alloc.cpp" ${C_SBIN} "" OFF)
endif(LINUX AND WANT_FULL_DEPLOYMENT)

if (WINDOWS AND WANT_FULL_DEPLOYMENT)
	condor_exe( procd_ctl "procd_ctl.cpp;${ProcClientElements};${SAFE_OPEN_SRC};../condor_utils/distribution.cpp;../condor_utils/my_distribution.cpp;../condor_utils/condor_pidenvid.cpp" ${C_SBIN} "procdutils;condor_utils" OFF)
endif(WINDOWS AND WANT_FULL_DEPLOYMENT)

set( PROCD_CLIENT_UTIL_SRCS "${ProcClientElements}" PARENT_SCOPE )
