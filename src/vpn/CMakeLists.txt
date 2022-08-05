 ###############################################################
 # 
 # Copyright 2020 Condor Team, Computer Sciences Department,
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

if (LINUX)

  set(VPN
	vpn_main.cpp
	vpn_common.cpp
	vpn_lease_mgr.cpp
  )

  set(VPN_CLIENT vpn_client.cpp vpn_common.cpp)

  condor_exe( condor_vpn "${VPN}" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )
  condor_exe( condor_vpn_client "${VPN_CLIENT}" ${C_BIN} "${CONDOR_LIBS}" OFF )

endif()

