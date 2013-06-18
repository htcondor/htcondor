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

  condor_glob( LARKHDRS LARKSRCS "ns_exec.c;network_netlink_main.cpp;dump_firewall.c;cleanup_firewall.c;ip_lock_main.cpp;dhcp_main.cpp;bridge_netlink.c;lark_plugin.cpp;bridge_tester.cpp;ovs_bridge_tester.cpp" )

  condor_exe( lark_dump_firewall "dump_firewall.c;network_manipulation.h;network_netfilter.c" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_ns_exec_tester "ns_exec.c" OFF "${CONDOR_TOOL_LIBS}" OFF)
  condor_exe( lark_network_namespace_tester "network_netlink_main.cpp;${LARKHDRS};${LARKSRCS};../../condor_utils/network_adapter.unix.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS};rt" OFF)
  condor_exe( lark_cleanup_firewall "cleanup_firewall.c;network_manipulation.h;network_netfilter.c" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_ip_lock_tester "ip_lock_main.cpp;ip_lock.cpp" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_dhcp_client_tester "dhcp_main.cpp;dhcp_management.cpp;dhcp_management.h;network_manipulation.h;network_netlink.cpp;bridge_manipulation.c;popen_util.h;bridge_routing.h;bridge_routing.cpp" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_bridge_tester "bridge_tester.cpp;bridge_manipulation.c;network_netlink.cpp;bridge_routing.cpp" OFF "${CONDOR_LIBS}" OFF )
  condor_exe( lark_ovs_bridge_tester "ovs_bridge_tester.cpp;ovs_bridge_manipulation.c;network_netlink.cpp;bridge_routing.cpp" OFF "${CONDOR_LIBS}" OFF)

  add_custom_command ( OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lark_cleanup_script.sh COMMAND ${CMAKE_COMMAND} ARGS -E copy ${CMAKE_CURRENT_SOURCE_DIR}/lark_cleanup_script.sh ${CMAKE_CURRENT_BINARY_DIR}/lark_cleanup_script.sh )
  add_custom_target ( lark_scripts ALL DEPENDS lark_cleanup_script.sh )

  condor_plugin( lark-plugin "${LARKHDRS};${LARKSRCS};lark_plugin.cpp" "${C_LIBEXEC}" "${CONDOR_TOOL_LIBS}" ON)

endif()
