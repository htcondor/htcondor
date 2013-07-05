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


condor_selective_glob("condor_submit_dag*;dagman_instantiate*" submitDAG)

condor_selective_glob("condor_dagman_metrics_reporter.cpp" metricsReporter)

condor_glob(DAGHdrs DAGSrcs "${submitDAG};${metricsReporter}")

condor_exe(condor_dagman "${DAGHdrs};${DAGSrcs}" ${C_BIN} "${CONDOR_LIBS}" ON)

condor_exe(condor_submit_dag "condor_submit_dag.cpp;dagman_multi_dag.cpp;dagman_recursive_submit.cpp" ${C_BIN} "${CONDOR_TOOL_LIBS}" OFF)

if(NOT WINDOWS AND WITH_CURL)
condor_exe(condor_dagman_metrics_reporter
	"condor_dagman_metrics_reporter.cpp;${DAGHdrs}"
	${C_LIBEXEC} "${CONDOR_TOOL_LIBS};${CURL_FOUND}" OFF)
endif()
