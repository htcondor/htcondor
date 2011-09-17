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


set(BENCH_LINK_LIBS "${TOOL_LINK_LIBS}")

file( GLOB SysApiRmvElements *.t.* dhry21b* *_main.cpp )

condor_glob(SysapiHeaderFiles SysapiSourceFiles "${SysApiRmvElements}" )

#create the library
condor_shared_lib(sysapi "${SysapiHeaderFiles};${SysapiSourceFiles}")

# there was a test target which was never used.
# it makes the most sense to hook in a UT here instead of integ test
condor_exe(condor_kflops "kflops_main.cpp" ${C_LIBEXEC} "utils;sysapi;condorapi;${BENCH_LINK_LIBS}" OFF)

# unfortunately libutils comes with undefined symbols introduced by
# libdaemon_core (even when linking libutils to it)
target_link_libraries(condor_kflops -Wl,--allow-shlib-undefined)

condor_exe(condor_mips "mips_main.cpp" ${C_LIBEXEC} "utils;sysapi;condorapi;${BENCH_LINK_LIBS}" OFF)

# this is needed because libsysapi as a circular dependency on libutils
target_link_libraries(condor_mips utils -Wl,--allow-shlib-undefined)

