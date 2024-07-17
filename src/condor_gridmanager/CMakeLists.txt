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

set(GMSRCS
arcjob.cpp
arcresource.cpp
azurejob.cpp
azureresource.cpp
basejob.cpp
baseresource.cpp
condorjob.cpp
condorresource.cpp
ec2-gahp-client.cpp
ec2job.cpp
ec2resource.cpp
gahp-client.cpp
gcejob.cpp
gceresource.cpp
generic-gahp-client.cpp
gridmanager.cpp
gridmanager_main.cpp
infnbatchjob.cpp
infnbatchresource.cpp
proxymanager.cpp
)

set(CMAKE_UNITY_BUILD False)
if ( LINUX )
   if ( NOT LIBUUID_FOUND )
     message(FATAL_ERROR "gridmanager requires uuid library")
   endif()
   if ( NOT HAVE_UUID_UUID_H )
     message(FATAL_ERROR "gridmanager requires uuid header")
   endif()
   include_directories("${HAVE_UUID_UUID_H}")
   condor_exe( condor_gridmanager "${GMSRCS}" ${C_SBIN} "${CONDOR_TOOL_LIBS};${LIBUUID_FOUND}" OFF )
else()
   condor_exe( condor_gridmanager "${GMSRCS}" ${C_SBIN} "${CONDOR_TOOL_LIBS}" OFF )
 endif()

if (NOT WINDOWS)
   install (FILES remote_gahp DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )

endif()
