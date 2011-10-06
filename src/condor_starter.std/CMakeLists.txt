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

	# include the other libs.
	file( GLOB RmvElements test* driver.cpp)
 
	condor_glob(Hdrs Srcs "${RmvElements}")

	condor_exe(condor_starter.std
	"${Hdrs};${Srcs};${CKPT_REFS};${CNDRSYSCL_REFS}" ${C_SBIN}
	"${CONDOR_TOOL_LIBS};std_local_ref;stdunivio;${KRB5_FOUND};${OPENSSL_FOUND};${PCRE_FOUND};${ZLIB_FOUND}"
	OFF)

endif(STD_UNIVERSE)
