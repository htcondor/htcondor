 ###############################################################
 # 
 # Copyright (C) 2024, Center for High Throughput Computing
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

condor_daemon( EXE condor_placementd SOURCES "placementd.cpp"
  LIBRARIES "${CONDOR_LIBS};${SQLITE3_LIB}" INSTALL "${C_SBIN}")
