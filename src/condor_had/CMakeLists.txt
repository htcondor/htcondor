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

set(HADSrcs
	StateMachine.cpp
	HAD.cpp
	Utils.cpp
)

condor_exe(condor_had "${HADSrcs}" ${C_SBIN} "${CONDOR_LIBS}" ON)

set(RepSrcs
	AbstractReplicatorStateMachine.cpp
	ReplicatorStateMachine.cpp
	HAD_Version.cpp
	Utils.cpp
	Replication.cpp
	FilesOperations.cpp
)

condor_exe(condor_replication "${RepSrcs}" ${C_SBIN} "${CONDOR_LIBS}" OFF)

set(TransferSrcs
	BaseReplicaTransferer.cpp
	UploadReplicaTransferer.cpp
	DownloadReplicaTransferer.cpp
	Utils.cpp
	Transferer.cpp
	FilesOperations.cpp
)

condor_exe(condor_transferer "${TransferSrcs}" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF)

