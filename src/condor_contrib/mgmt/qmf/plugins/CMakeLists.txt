 ###############################################################
 # 
 # Copyright (C) 1990-2011, Redhat. 
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

set( QMF_GEN_SRCS Slot.h;
    Scheduler.h;
    Submitter.h;
    Negotiator.h;
    Collector.h;
    Master.h;
    Grid.h;
    Submission.h;
    Slot.cpp;
    Scheduler.cpp;
    Submitter.cpp;
    Negotiator.cpp;
    Collector.cpp;
    Master.cpp;
    Grid.cpp;
    Submission.cpp;
    JobServer.cpp;
    ArgsSchedulerSubmitJob.h;
    ArgsSchedulerGetJobAd.h;
    ArgsSchedulerSetJobAttribute.h;
    ArgsSchedulerHoldJob.h;
    ArgsSchedulerReleaseJob.h;
    ArgsSchedulerRemoveJob.h;
    ArgsSchedulerFetchJobData.h;
    ArgsSchedulerGetJobSummaries.h;
    ArgsSchedulerEcho.h;
    ArgsNegotiatorGetLimits.h;
    ArgsNegotiatorSetLimit.h;
    ArgsNegotiatorGetStats.h;
    ArgsNegotiatorSetPriority.h;
    ArgsNegotiatorSetPriorityFactor.h;
    ArgsNegotiatorSetUsage.h;
    ArgsNegotiatorGetRawConfig.h;
    ArgsNegotiatorSetRawConfig.h;
    ArgsMasterStart.h;
    ArgsMasterStop.h;
    Package.h;
    Package.cpp )

# qmf gen & copy out the results for out of source builds
add_custom_command( OUTPUT ${QMF_GEN_SRCS} 
                    COMMAND python
                    ARGS ${QMF_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/condor-management-schema.xml && cd com/redhat/grid && mv * ../../.. )

# used for selective compilation differences only otherwise 
# just link the library
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

condor_static_lib( qmf_utils "Utils.h;Utils.cpp;PoolUtils.cpp;PoolUtils.h" )

file( GLOB SchedSrcs JobUtils.* SchedulerObject.* SubmitterObject.* SubmitterUtils.* PROC_ID_comparator.* SubmissionObject.* JobServerObject.* MgmtScheddPlugin* )
condor_plugin( MgmtScheddPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${SchedSrcs};Scheduler.cpp;Submitter.cpp;Submission.cpp;JobServer.cpp" "qmf_utils" "${C_LIBEXEC}" ON)

#condor_plugin( MgmtCollectorPlugin-plugin ";" "${C_LIBEXEC}")
#condor_plugin( MgmtStartdPlugin-plugin ";" "${C_LIBEXEC}")
#condor_plugin( MgmtNegotiatorPlugin-plugin ";" "${C_LIBEXEC}")
#condor_plugin( MgmtMasterPlugin-plugin "utils;" "${C_LIBEXEC}")

