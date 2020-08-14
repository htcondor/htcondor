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

set(SysapiSourceFiles
${CMAKE_CURRENT_SOURCE_DIR}/arch.cpp
${CMAKE_CURRENT_SOURCE_DIR}/free_fs_blocks.cpp
${CMAKE_CURRENT_SOURCE_DIR}/idle_time.cpp
${CMAKE_CURRENT_SOURCE_DIR}/kernel_version.cpp
${CMAKE_CURRENT_SOURCE_DIR}/last_x_event.cpp
${CMAKE_CURRENT_SOURCE_DIR}/load_avg.cpp
${CMAKE_CURRENT_SOURCE_DIR}/ncpus.cpp
${CMAKE_CURRENT_SOURCE_DIR}/net_dev_info.cpp
${CMAKE_CURRENT_SOURCE_DIR}/partition_id.cpp
${CMAKE_CURRENT_SOURCE_DIR}/phys_mem.cpp
${CMAKE_CURRENT_SOURCE_DIR}/processor_flags.cpp
${CMAKE_CURRENT_SOURCE_DIR}/reconfig.cpp
${CMAKE_CURRENT_SOURCE_DIR}/resource_limits.cpp
${CMAKE_CURRENT_SOURCE_DIR}/virt_mem.cpp
)

# clinpack comes from a 3rd party, we don't ever want to touch it, even
# to fix warnings.
if (UNIX)
set_source_files_properties(clinpack.cpp PROPERTIES COMPILE_FLAGS "-w")
endif(UNIX)

set( SYSAPI_UTIL_SRCS "${SysapiSourceFiles}" PARENT_SCOPE )

# there was a test target which was never used.
# it makes the most sense to hook in a UT here instead of integ test
condor_exe(condor_kflops "kflops_main.cpp;clinpack.cpp;../condor_utils/utc_time.cpp" ${C_LIBEXEC} "" OFF)

if (NOT WINDOWS)
    # compiling everything with -fPIC is needed to dynamically load libraries, but it messes up the benchmarks so turn it back off here
    set_property(SOURCE clinpack.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -fno-PIC -fPIE")
    set_property(SOURCE dhry21a.cpp APPEND_STRING PROPERTY COMPILE_FLAGS " -fno-PIC -fPIE")
endif (NOT WINDOWS)

condor_exe(condor_mips "mips_main.cpp;dhry21a.cpp;../condor_utils/utc_time.cpp" ${C_LIBEXEC} "" OFF)
