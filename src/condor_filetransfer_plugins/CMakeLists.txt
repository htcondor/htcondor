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

#  Curl isn't linking properly on windows, turn it as a short term hackaround
if (CURL_FOUND)
  if (WINDOWS)
    condor_exe( curl_plugin "multifile_curl_plugin.cpp;../condor_utils/file_transfer_stats.cpp" ${C_LIBEXEC} "${CURL_FOUND};wldap32.lib;${CONDOR_TOOL_LIBS}" OFF )
  else ()
    condor_exe( curl_plugin "multifile_curl_plugin.cpp;../condor_utils/file_transfer_stats.cpp" ${C_LIBEXEC} "${CURL_FOUND};${CONDOR_TOOL_LIBS}" OFF )
  endif ()
endif()

condor_exe( data_plugin "data_plugin.c" ${C_LIBEXEC} "" OFF)
