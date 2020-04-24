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

if (LINUX)

set(Srcs
getPort.cpp
pigeon.cpp
pigeon_main.cpp
)

condor_static_lib( condorPigeon "${Hdrs};${Srcs}" ) 
condor_exe( declareQueues "declare_queues.cpp" "${C_SBIN}" "condorPigeon;condorapi;${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND};${QPID_FOUND}" OFF )
condor_daemon( pigeon  "${RmvSrcs}" "condorPigeon;${CONDOR_LIBS};${OPENSSL_FOUND};${KRB5_FOUND};${QPID_FOUND}" "${C_SBIN}")

endif(LINUX)
