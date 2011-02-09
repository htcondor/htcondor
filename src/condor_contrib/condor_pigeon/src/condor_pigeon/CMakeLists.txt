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
include_directories ( ${EXTERNAL_STAGE}/include/qpid )
link_directories ( ${EXTERNAL_STAGE}/lib/qpid )

file( GLOB RmvSrcs *_main* declare_queues.* )
condor_glob( Hdrs Srcs "${RmvSrcs}" )

condor_static_lib( condorPigeon "${Hdrs};${Srcs}" ) 
condor_exe( declareQueues "declare_queues.cpp" "${C_SBIN}"
"condorPigeon;condorapi;daemon_core;${CONDOR_TOOL_LIBS};${QPID_FOUND};${DAEMON_CORE}" OFF )
condor_daemon( pigeon "pigeon_main.cpp" "${C_SBIN}"
"condorPigeon;condorapi;daemon_core;${CONDOR_TOOL_LIBS};${CONDOR_LIBS};${QPID_FOUND};${DAEMON_CORE}" OFF )

endif(LINUX)
