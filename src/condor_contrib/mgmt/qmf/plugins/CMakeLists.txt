 ###############################################################
 # 
 # Copyright (C) 2009-2011, Red Hat, Inc.
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
    #ArgsSchedulerSubmitJob.h;
    #ArgsSchedulerGetJobAd.h;
    #ArgsSchedulerSetJobAttribute.h;
    #ArgsSchedulerHoldJob.h;
    #ArgsSchedulerReleaseJob.h;
    #ArgsSchedulerRemoveJob.h;
    #ArgsSchedulerFetchJobData.h;
    #ArgsSchedulerGetJobSummaries.h;
    #ArgsSchedulerEcho.h;
    #ArgsNegotiatorGetLimits.h;
    #ArgsNegotiatorSetLimit.h;
    #ArgsNegotiatorGetStats.h;
    #ArgsNegotiatorSetPriority.h;
    #ArgsNegotiatorSetPriorityFactor.h;
    #ArgsNegotiatorSetUsage.h;
    #ArgsNegotiatorGetRawConfig.h;
    #ArgsNegotiatorSetRawConfig.h;
    #ArgsMasterStart.h;
    #ArgsMasterStop.h;
    #ArgsGridMethodsEcho.h;
    #ArgsJobServerFetchJobData.h;
    #ArgsJobServerGetJobAd.h;
    #ArgsSubmissionGetJobSummaries.h;
    Package.h;
    Package.cpp )

# qmf gen & copy out the results for out of source builds
add_custom_command( OUTPUT ${QMF_GEN_SRCS} 
                    COMMAND python
                    ARGS ${QMF_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/condor-management-schema.xml && cd com/redhat/grid && mv -f * ../../.. 
                    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/condor-management-schema.xml )

add_custom_target (qmf_gen_srcs
                    ALL
                    DEPENDS ${QMF_GEN_SRCS} )

add_dependencies( qmf_gen_srcs qpid )

include_directories(${CMAKE_CURRENT_BINARY_DIR})
src_target_ref( broker_utils "^.*$" BROKER_REF )
src_target_ref( utils "set_user_priv_from_ad;hashkey" UTILS_REF )

if (NOT WINDOWS)

	condor_static_lib( qmf_utils "Utils.h;Utils.cpp;PoolUtils.cpp;PoolUtils.h" )

	# NOTE: Still need to update linkage deps
	file( GLOB SchedSrcs JobUtils.* SchedulerObject.* SubmitterObject.* SubmitterUtils.* PROC_ID_comparator.* SubmissionObject.* JobServerObject.* MgmtScheddPlugin* Utils.* PoolUtils.*)
    condor_plugin( MgmtScheddPlugin-plugin "${UTILS_REF};${BROKER_REF};${SchedSrcs};Scheduler.cpp;Submitter.cpp;Submission.cpp;JobServer.cpp" "${C_LIBEXEC}" "${QPID_FOUND}" ON)
    add_dependencies( MgmtScheddPlugin-plugin qmf_gen_srcs )

	file( GLOB CollectorSrcs MgmtCollectorPlugin.* GridObject.* NegotiatorObject.* CollectorObject.* SlotObject.* PoolUtils.* SchedulerObject.*)
	condor_plugin( MgmtCollectorPlugin-plugin "${BROKER_REF};${CollectorSrcs};Grid.cpp;Collector.cpp;Slot.cpp;Negotiator.cpp;Scheduler.cpp" "${C_LIBEXEC}" "${QPID_FOUND}" ON)
	set_property(TARGET MgmtCollectorPlugin-plugin APPEND PROPERTY COMPILE_DEFINITIONS "READ_ONLY_NEGOTIATOR_OBJECT=1")
	set_property(TARGET MgmtCollectorPlugin-plugin APPEND PROPERTY COMPILE_DEFINITIONS "READ_ONLY_SCHEDULER_OBJECT=1")
    add_dependencies( MgmtCollectorPlugin-plugin qmf_gen_srcs )

	file( GLOB NegotiatorSrcs MgmtNegotiatorPlugin.* NegotiatorObject.* Utils.* PoolUtils.*)
	condor_plugin( MgmtNegotiatorPlugin-plugin "${BROKER_REF};${NegotiatorSrcs};Negotiator.cpp" "${C_LIBEXEC}" "${QPID_FOUND}" ON)
    add_dependencies( MgmtNegotiatorPlugin-plugin qmf_gen_srcs )

	file( GLOB MasterSrcs MgmtMasterPlugin.* MasterObject.* PoolUtils.* )
	condor_plugin( MgmtMasterPlugin-plugin "${BROKER_REF};${MasterSrcs};Master.cpp" "${C_LIBEXEC}" "${QPID_FOUND}" ON)
    add_dependencies( MgmtMasterPlugin-plugin qmf_gen_srcs )

	file( GLOB StartdSrcs MgmtStartdPlugin.* SlotObject.* PoolUtils.*)
	condor_plugin( MgmtStartdPlugin-plugin "${UTILS_REF};${BROKER_REF};${StartdSrcs};Slot.h;Slot.cpp" "${C_LIBEXEC}" "${QPID_FOUND}" ON)
    add_dependencies( MgmtStartdPlugin-plugin qmf_gen_srcs )
	
else()

	file( GLOB QMFRMV ${QMF_GEN_SRCS} )
	condor_glob( HDRS SRCS "${QMFRMV}" )
	
	condor_static_lib( condor_qmflib "${HDRS};${SRCS};${QMF_GEN_SRCS};${BROKER_REF};${UTILS_REF}" )
	add_dependencies( condor_qmflib qmf_gen_srcs )

endif()
