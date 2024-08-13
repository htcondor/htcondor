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

# Just doesn't work here, too much tomfoolery
set(CMAKE_UNITY_BUILD False)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(qmgmtElements
	${CMAKE_CURRENT_SOURCE_DIR}/qmgmt_send_stubs.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/qmgr_lib_support.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/qmgr_job_updater.cpp
)

set(scheddElements
autocluster.cpp
checkpoint_cleanup.cpp
dedicated_scheduler.cpp
grid_universe.cpp
ickpt_share.cpp
jobsets.cpp
job_transforms.cpp
pccc.cpp
qmgmt_common.cpp
qmgmt.cpp
qmgmt_factory.cpp
qmgmt_receivers.cpp
schedd.cpp
schedd_cron_job.cpp
schedd_cron_job_mgr.cpp
schedd_main.cpp
schedd_negotiate.cpp
ScheddPluginManager.cpp
schedd_stats.cpp
transfer_queue.cpp
)

# There's a tricky warning in here that I don't know how
# to fix yet
set_source_files_properties(schedd_negotiate.cpp PROPERTIES COMPILE_OPTIONS "-Wno-error")

condor_daemon( EXE condor_schedd SOURCES "${scheddElements}"
  LIBRARIES "${CONDOR_LIBS}" INSTALL "${C_SBIN}")

set( QMGMT_UTIL_SRCS "${qmgmtElements};${CMAKE_CURRENT_SOURCE_DIR}/qmgmt_common.cpp" PARENT_SCOPE )
