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

if(STD_UNIVERSE )

	add_definitions(-DSYSCALL_DEBUG)
	include_directories(${CONDOR_SOURCE_DIR}/src/condor_shadow.std)

	condor_glob(Hdrs Srcs "")

	condor_exe(condor_shadow.std
	"${Hdrs};${Srcs};${CNDRSYSCL_REFS}" ${C_SBIN}
	"${CONDOR_TOOL_LIBS};std_local_ref;syscall_numbers;stdunivio;${KRB5_FOUND};${OPENSSL_FOUND};${PCRE_FOUND};${ZLIB_FOUND}"
	OFF)

endif(STD_UNIVERSE)
