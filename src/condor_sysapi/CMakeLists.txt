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

file( GLOB SysApiRmvElements *_t.cpp *.t.* dhry21b* *_main.cpp )

condor_glob(SysapiHeaderFiles SysapiSourceFiles "${SysApiRmvElements}" )

#create the library
condor_static_lib(sysapi "${SysapiHeaderFiles};${SysapiSourceFiles}")

# there was a test target which was never used.
# it makes the most sense to hook in a UT here instead of integ test
condor_exe(condor_kflops "kflops_main.cpp" ${C_LIBEXEC} "sysapi;${CONDOR_TOOL_LIBS}" OFF)
condor_exe(condor_mips "mips_main.cpp" ${C_LIBEXEC} "sysapi;${CONDOR_TOOL_LIBS}" OFF)
