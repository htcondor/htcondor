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


file( GLOB MasterRmvElements Example* )

condor_daemon(master "${MasterRmvElements}" "${CONDOR_LIBS};${CONDOR_QMF}" "${C_SBIN}" OFF)
# We cannot make a proper static master as we don't have the static globus
# libraries available
if (NOT PROPER AND LINUX)
  condor_daemon(master_s "${MasterRmvElements}" "${CONDOR_LIBS_STATIC};${CONDOR_QMF}" "${C_SBIN}" OFF)
endif(NOT PROPER AND LINUX)
