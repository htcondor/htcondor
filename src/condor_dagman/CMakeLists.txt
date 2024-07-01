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

set(DAGSrcs 
dag.cpp
dagman_classad.cpp
dagman_commands.cpp
dagman_main.cpp
dagman_metrics.cpp
dagman_stats.cpp
dagman_submit.cpp
debug.cpp
node.cpp
jobstate_log.cpp
parse.cpp
script.cpp
scriptQ.cpp
throttle_by_category.cpp
)

condor_exe(condor_dagman "${DAGSrcs}" ${C_BIN} "${CONDOR_LIBS}" ON)

condor_exe(condor_submit_dag "condor_submit_dag.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)
