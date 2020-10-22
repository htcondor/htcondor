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

# for qmgr_job_updaters
include_directories(${CONDOR_SCHEDD_SRC_DIR})

set(shadowElements
baseshadow.cpp
mpiresource.cpp
NTreceivers.cpp
parallelshadow.cpp
pseudo_ops.cpp
remoteresource.cpp
shadow.cpp
shadow_user_policy.cpp
shadow_v61_main.cpp
uuid_stubs.cpp
)


condor_daemon( EXE condor_shadow SOURCES "${shadowElements}" LIBRARIES "${CONDOR_LIBS_FOR_SHADOW};${CMAKE_DL_LIBS}" INSTALL "${C_SBIN}" )

