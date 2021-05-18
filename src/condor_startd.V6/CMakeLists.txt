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


if (DARWIN)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework IOKit -framework Carbon")
endif()

set(startdElements
backfill_mgr.cpp
claim.cpp
cod_mgr.cpp
command.cpp
IdDispenser.cpp
LoadQueue.cpp
Reqexp.cpp
ResAttributes.cpp
ResMgr.cpp
Resource.cpp
ResState.cpp
slot_builder.cpp
startd_bench_job.cpp
startd_bench_job_mgr.cpp
startd_bench_job_params.cpp
startd_cron_job.cpp
startd_cron_job_mgr.cpp
startd_cron_job_params.cpp
startd_hibernator.cpp
StartdHookMgr.cpp
startd_main.cpp
startd_named_classad.cpp
startd_named_classad_list.cpp
StartdPluginManager.cpp
Starter.cpp
starter_mgr.cpp
util.cpp
vm_common.cpp
VMMachine.cpp
VMManager.cpp
VMRegister.cpp
vmuniverse_mgr.cpp
)

if (WINDOWS)
	list(APPEND startdElements
		CondorSystrayNotifier.windows.cpp
		winreg.windows.cpp
	)
endif(WINDOWS)

if (LINUX)
	list(APPEND startdElements
		glexec_starter.linux.cpp
	)
endif(LINUX)


if (HAVE_BOINC)
list(APPEND startdElements boinc_mgr.cpp)
endif()

condor_daemon( EXE condor_startd SOURCES "${startdElements}" LIBRARIES "${CONDOR_LIBS};${CONDOR_QMF}" INSTALL "${C_SBIN}")

if (LINUX AND GLOBUS_FOUND AND WANT_GLEXEC)
  condor_exe(condor_glexec_wrapper "glexec_wrapper.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS}" OFF )
  install (FILES glexec_starter_setup.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
endif()
