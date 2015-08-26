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

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)
include_directories(${CONDOR_SCHEDD_SRC_DIR})

############################
# file target references.
############################
file( GLOB JOB_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../../codegen/job/src/*.cpp )
file( GLOB JOB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/AviaryJobService*.cpp )

file( GLOB SCHED_PLUGIN_SRCS ../common/ClassadCodec.cpp ../common/AviaryUtils.cpp SchedulerObject.cpp
        AviaryScheddPlugin*.cpp )


############################
# schedd plugin
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

add_library (aviary_job_axis SHARED ${JOB_GEN_SRCS} ${JOB_SRCS} 
                ../common/ClassadCodec.cpp ../common/AviaryUtils.cpp SchedulerObject.cpp )
# remove unresolved deps by copying in -lwso2_wsf objects
target_link_libraries(aviary_job_axis -Wl,--whole-archive -lwso2_wsf -Wl,--no-whole-archive)
target_link_libraries(aviary_job_axis aviary_wso2_common)

condor_plugin( AviaryScheddPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${SCHED_PLUGIN_SRCS}" "${C_LIBEXEC}" "${WSO2_FOUND}" ON)
# remove unresolved deps by copying in -laxis2_engine objects
target_link_libraries(AviaryScheddPlugin-plugin -Wl,--whole-archive -laxis2_engine -Wl,--no-whole-archive)
# link externally to provider and common
target_link_libraries(AviaryScheddPlugin-plugin aviary_axis_provider)
target_link_libraries(AviaryScheddPlugin-plugin aviary_wso2_common)


############################
# install target
############################
install (TARGETS aviary_job_axis DESTINATION ${C_SERVICES}/job)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../services/job/services.xml DESTINATION ${C_SERVICES}/job )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-common.xsd
            ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-job.xsd
            ${CMAKE_CURRENT_SOURCE_DIR}/../../etc/aviary-job.wsdl
            DESTINATION ${C_SERVICES}/job )
