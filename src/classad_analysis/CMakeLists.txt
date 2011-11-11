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


condor_selective_glob( "analysis.*;boolExpr.*;condition.*;multiProfile.*;profile.*;resourceGroup.*;boolValue.*;explain.*;interval.*;result.*" AnalSrcs )

set_source_files_properties(interval.cpp PROPERTIES COMPILE_FLAGS
-Wno-float-equal)
set( ANALYSIS_UTIL_SRCS "${AnalSrcs};${CMAKE_CURRENT_SOURCE_DIR}/conversion.cpp;${CMAKE_CURRENT_SOURCE_DIR}/conversion.h" PARENT_SCOPE)

# # condor_shared_lib( gangmatch "portGraph.cpp;gmr.cpp;gangster.cpp")
