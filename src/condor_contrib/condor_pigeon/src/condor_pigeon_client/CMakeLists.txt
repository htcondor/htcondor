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
modTrial.cpp
)

condor_exe( daemonReader "daemonReader.cpp" "${C_SBIN}" "condorPigeon;${CONDOR_TOOL_LIBS};${OPENSSL_FOUND};${KRB5_FOUND};${QPID_FOUND}" OFF )
condor_exe( condor_pigeon_topic_publisher "topic_publisher.cpp;${Srcs}" "${C_BIN}" "condorapi;${CONDOR_TOOL_LIBS};${QPID_FOUND}" OFF )
condor_exe( condor_pigeon_topic_listener "topic_listener.cpp" "${C_BIN}" "${QPID_FOUND}" OFF )

###comment the Skype part out for now, since it would require some file copying -- it should be enough to give an idea of what's possible
##condor_exe( condor_pigeon_topic_listener_skype "topic_listener_skype.cpp" "${C_BIN}" "${QPID_FOUND}" OFF )

endif(LINUX)
