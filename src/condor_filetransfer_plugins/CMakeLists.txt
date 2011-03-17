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


#file( GLOB HOOK hdfs_plugin.* hdfs_plugin_main.*)
#file( GLOB HDFSLIB hdfs.* hdfsJniHelper.* )

#add_library( hdfs SHARED ${HDFSLIB} )

#condor_exe( condor_hdfs_hook "${HOOK}" ${C_SBIN} "hdfs;${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${CLASSADS_FOUND};${COREDUMPER_FOUND}" OFF)

dprint( "(TSTCLAIR)-> Deal with Java deps and FT HOOK PLUGINS also what is up with this? it's not in packages...")

#  Curl isn't linking properly on windows, turn it as a short term hackaround
if (CURL_FOUND AND NOT WINDOWS)
condor_exe( curl_plugin "curl_plugin.c" ${C_LIBEXEC}
"${CURL_FOUND};${LDAP_FOUND};${OPENSSL_FOUND};${ZLIB_FOUND};${KRB5_FOUND}" OFF )
endif()

condor_exe( data_plugin "data_plugin.c" ${C_LIBEXEC} "" OFF)
