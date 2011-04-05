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

#############################
set( QMF_GEN_SRCS #ArgsCondorTriggerGetTriggerId.h;
      ArgsCondorTriggerGetTriggerName.h;
      ArgsCondorTriggerGetTriggerQuery.h;
      ArgsCondorTriggerGetTriggerText.h;
      ArgsCondorTriggerServiceAddTrigger.h;
      ArgsCondorTriggerServiceGetEvalInterval.h;
      ArgsCondorTriggerServiceRemoveTrigger.h;
      ArgsCondorTriggerServiceSetEvalInterval.h;
      ArgsCondorTriggerServiceSetTriggerName.h;
      ArgsCondorTriggerServiceSetTriggerQuery.h;
      ArgsCondorTriggerServiceSetTriggerText.h;
      CondorTrigger.cpp;
      CondorTrigger.h;
      CondorTriggerService.cpp;
      CondorTriggerService.h;
      EventCondorTriggerNotify.cpp;
      EventCondorTriggerNotify.h;
      Package.cpp;
      Package.h )

    # qmf gen & copy out the results for out of source builds
add_custom_command( OUTPUT ${QMF_GEN_SRCS} 
                    COMMAND python
                    ARGS ${QMF_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/condor-trigger-schema.xml && cd com/redhat/grid && mv * ../../.. )

# needed if doing out of source builds to include generated code.
include_directories(${CMAKE_CURRENT_BINARY_DIR})
#############################


file( GLOB Srcs TriggerConsole.* Triggerd.* TriggerObject.* triggerd_main.cpp )
condor_exe( condor_triggerd "${Srcs};CondorTrigger.cpp;CondorTriggerService.cpp;EventCondorTriggerNotify.cpp" "${C_SBIN}" "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${QPID_FOUND};broker_utils" OFF )

#############################

install ( FILES get_trigger_data DESTINATION ${C_BIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
install ( FILES condor_trigger_config.py DESTINATION ${C_SBIN} PERMISSIONS ${CONDOR_SCRIPT_PERMS} RENAME condor_trigger_config )
