 ###############################################################
 # 
 # Copyright (C) 1990-2011, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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

option(WITH_LARK "Support for Lark network tools" OFF)

if ( WITH_LARK )

  condor_glob( LARKHDRS LARKSRCS "ns_exec.c;network_netlink_main.cpp;dump_firewall.c;cleanup_firewall.c;ip_lock_main.cpp" )

  condor_exe( lark_dump_firewall "dump_firewall.c;${LARKHDRS};${LARKSRCS}" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_ns_exec_tester "ns_exec.c" OFF "${CONDOR_TOOL_LIBS}" OFF)
  condor_exe( lark_network_namespace_tester "network_netlink_main.cpp;${LARKHDRS};${LARKSRCS}" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS}" OFF)
  condor_exe( lark_cleanup_firewall "cleanup_firewall.c;${LARKHDRS};${LARKSRCS}" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_ip_lock_tester "ip_lock_main.cpp;ip_lock.cpp" OFF "${CONDOR_LIBS}" OFF )

  condor_plugin( lark-plugin "${LARKHDRS};${LARKSRCS};network_namespaces.cpp" "${C_LIBEXEC}" "${CONDOR_TOOL_LIBS}" ON)

endif()
