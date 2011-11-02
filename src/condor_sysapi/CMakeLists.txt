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

file( GLOB SysApiRmvElements *_t.* *.t.* dhry21b* *_main.cpp test.* )


condor_glob(SysapiHeaderFiles SysapiSourceFiles "${SysApiRmvElements}" )

# clinpack comes from a 3rd party, we don't every want to touch it, even
# to fix warnings.
if (UNIX)
set_source_files_properties(clinpack.cpp PROPERTIES COMPILE_FLAGS -w)
endif(UNIX)

set( SYSAPI_UTIL_SRCS "${SysapiHeaderFiles};${SysapiSourceFiles}" PARENT_SCOPE )

# there was a test target which was never used.
# it makes the most sense to hook in a UT here instead of integ test
condor_exe(condor_kflops "kflops_main.cpp;clinpack.cpp;../condor_utils/utc_time.cpp" ${C_LIBEXEC} "" OFF)

condor_exe(condor_mips "mips_main.cpp;dhry21a.cpp;../condor_utils/utc_time.cpp" ${C_LIBEXEC} "" OFF)
