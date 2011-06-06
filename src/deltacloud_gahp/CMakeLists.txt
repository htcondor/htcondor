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


if (HAVE_EXT_LIBDELTACLOUD)

	condor_exe( deltacloud_gahp "dcloudgahp_main.cpp;dcloudgahp_commands.cpp;PipeBuffer.cpp;dcloudgahp_common.cpp" ${C_SBIN}
	"${LIBDELTACLOUD_FOUND};${LIBXML2_FOUND};${CURL_FOUND};${OPENSSL_FOUND};${LDAP_FOUND}" OFF )

else()

	message(STATUS "NOTE: *deltacloud* targets will not be built HAVE_EXT_LIBDELTACLOUD=${HAVE_EXT_LIBDELTACLOUD}")

endif (HAVE_EXT_LIBDELTACLOUD)
