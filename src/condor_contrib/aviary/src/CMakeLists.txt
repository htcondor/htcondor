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

# shouldn't happen if gen code checked-in
if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../codegen" )
     message( FATAL_ERROR "run ../gen-xsd-cpp.sh first!")
endif ()

############################
# file target references.
############################
file( GLOB SOAP_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryProviderFactory.cpp Axis2SoapProvider.cpp)
file( GLOB COMMON_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/common/src/*.cpp )
file( GLOB JOB_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/job/src/*.cpp )
file( GLOB JOB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/AviaryJobService*.cpp )
file( GLOB QUERY_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/query/src/*.cpp )
file( GLOB QUERY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryQueryService*.cpp )

if (WITH_OPENSSL)
    file( GLOB SSL_SRCS ${CMAKE_CURRENT_SOURCE_DIR} axis2_ssl_*.c Axis2SslProvider.cpp)
    list(APPEND SOAP_SRCS ${SSL_SRCS} )
endif(WITH_OPENSSL)

file( GLOB SCHED_PLUGIN_SRCS Codec.cpp AviaryUtils.cpp SchedulerObject.cpp
			PROC_ID_comparator.cpp ${SOAP_SRCS} AviaryScheddPlugin*.cpp )

file( GLOB JOB_SERVER_SRCS aviary_query_server.cpp ${SOAP_SRCS}
							SubmissionObject.cpp JobServerJobLogConsumer.cpp
							Job.cpp HistoryFile.cpp HistoryProcessingUtils.cpp Globals.cpp
							Codec.cpp JobServerObject.cpp AviaryUtils.cpp )

############################
# schedd plugin
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

add_library (aviary_job_axis SHARED ${COMMON_GEN_SRCS} ${JOB_GEN_SRCS} ${JOB_SRCS} Codec.cpp AviaryUtils.cpp SchedulerObject.cpp )
# remove unresolved deps by copying in -lwso2_wsf objects
target_link_libraries(aviary_job_axis -Wl,--whole-archive -lwso2_wsf -Wl,--no-whole-archive)

condor_plugin( AviaryScheddPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${SCHED_PLUGIN_SRCS}" "${C_LIBEXEC}" "${WSO2_FOUND}" ON)
# remove unresolved deps by copying in -laxis2_engine objects
target_link_libraries(AviaryScheddPlugin-plugin -Wl,--whole-archive -laxis2_engine -Wl,--no-whole-archive)

############################
# query server (aka ye old job server)
############################
add_library (aviary_query_axis SHARED ${COMMON_GEN_SRCS} ${QUERY_GEN_SRCS} ${QUERY_SRCS} )
target_link_libraries (aviary_query_axis ${WSO2_FOUND})

condor_exe( aviary_query_server "${JOB_SERVER_SRCS}"
			"${C_SBIN}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${WSO2_FOUND}" OFF )

############################
# install targets && configure our axis2.xml for platform
############################
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../etc/axis2.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/../etc/axis2.xml" @ONLY)
install (FILES ${CMAKE_CURRENT_BINARY_DIR}/../etc/axis2.xml DESTINATION ${CMAKE_INSTALL_PREFIX} )

install (TARGETS aviary_query_axis DESTINATION ${C_SERVICES}/query)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../services/query/services.xml DESTINATION ${C_SERVICES}/query )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-common.xsd
			${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.xsd
			${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.wsdl
		DESTINATION ${C_SERVICES}/query )

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/61aviary.config DESTINATION "${C_ETC_EXAMPLES}")
file(GLOB TEST_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../test/*.py)
file(GLOB DAG_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../test/dag/*)
file(GLOB SUPPORT_SCRIPTS ${CMAKE_CURRENT_SOURCE_DIR}/../test/module/aviary/*)
install(FILES ${TEST_SCRIPTS} DESTINATION ${C_LIB}/aviary PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${DAG_SCRIPTS} DESTINATION ${C_LIB}/aviary/dag PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${SUPPORT_SCRIPTS} DESTINATION ${C_LIB}/aviary/module/aviary PERMISSIONS ${CONDOR_SCRIPT_PERMS})
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../README DESTINATION ${C_LIB}/aviary)

install (TARGETS aviary_job_axis DESTINATION ${C_SERVICES}/job)
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../services/job/services.xml DESTINATION ${C_SERVICES}/job )
install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-common.xsd
				${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-job.xsd
				${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-job.wsdl
		DESTINATION ${C_SERVICES}/job )

