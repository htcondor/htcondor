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

if (LINUX)

    if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../codegen" )
        message( FATAL_ERROR "run ../gen-build-xsd-lib.sh first!")
    endif ()

    if ( PROPER )

		find_multiple( "axutil;axis2_axiom;axis2_parser;axis2_http_common;axis2_http_receiver;axis2_http_sender;neethi;guththila;axis2_engine;wso2_wsf" WSO2_FOUND )
       message (STATUS "external configured WSO2_FOUND=${WSO2_FOUND}")

       include_directories( /usr/include/axis2-1.6.0
			    /usr/include/wso2-wsf-cpp
			    /usr/include/axis2-1.6.0/platforms )
	    set ( AXIS2_LIBS -L/usr/lib -L/usr/lib64 ${WSO2_FOUND} )

		 set(aviary_services_loc ${CMAKE_INSTALL_PREFIX}/axis2/services)

	else (PROPER)

		# TODO: querying env vars not reliable like this
	    if ( NOT DEFINED ENV{WSFCPP_HOME} )
	        message( FATAL_ERROR "$WSFCPP_HOME not defined for non-proper build")
	    endif ()

		set ( AXIS2_LIBS -L$ENV{WSFCPP_HOME}/lib
		     -laxutil -laxis2_axiom -laxis2_parser
		     -laxis2_http_common -laxis2_http_receiver
		     -laxis2_http_sender -lneethi -lguththila
		     -laxis2_engine -lwso2_wsf )

		include_directories(
			    $ENV{WSFCPP_HOME}/include/axis2-1.6.0
			    $ENV{WSFCPP_HOME}/include/wso2-wsf-cpp
			    $ENV{WSFCPP_HOME}/include/axis2-1.6.0/platforms )

        set(aviary_services_loc $ENV{WSFCPP_HOME}/services)

	endif ( PROPER )

    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

    # path to generated headers
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )

    file( GLOB COMMON_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/common/src/*.cpp )

    file( GLOB JOB_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/job/src/*.cpp )
    file( GLOB JOB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/AviaryJobService*.cpp )

	file( GLOB QUERY_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/query/src/*.cpp )
    file( GLOB QUERY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryQueryService*.cpp )

    # used for selective compilation differences only otherwise just link the library
    src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
    src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

    add_library (aviary_common SHARED ${COMMON_GEN_SRCS} )

    add_library (aviary_job SHARED ${JOB_GEN_SRCS} ${JOB_SRCS} Codec.cpp SchedulerObject.cpp AviaryUtils.cpp)
    target_link_libraries (aviary_job aviary_common ${AXIS2_LIBS} )
    if (aviary_services_loc)
        install (TARGETS aviary_common DESTINATION ${aviary_services_loc}/job)
		install (TARGETS aviary_job DESTINATION ${aviary_services_loc}/job)
    endif()

    file( GLOB SCHED_PLUGIN_SRCS AviaryUtils.cpp PROC_ID_comparator.cpp SchedulerObject.cpp Axis2SoapProvider.cpp AviaryScheddPlugin*.cpp )

    add_library( AviaryScheddPlugin-plugin SHARED ${COLLECTOR_REF} ${UTILS_REF} ${SCHED_PLUGIN_SRCS})
    target_link_libraries (AviaryScheddPlugin-plugin aviary_job ${AXIS2_LIBS})
    set(AviaryScheddPlugin-plugin_loc ${C_LIBEXEC})
    if (AviaryScheddPlugin-plugin_loc)
        install (TARGETS AviaryScheddPlugin-plugin DESTINATION ${C_LIBEXEC} )
    endif()
    set_property( TARGET AviaryScheddPlugin-plugin PROPERTY PREFIX "" )

	add_library (aviary_query SHARED ${QUERY_GEN_SRCS} ${QUERY_SRCS} Codec.cpp JobServerObject.cpp AviaryUtils.cpp )
	target_link_libraries (aviary_query aviary_common ${AXIS2_LIBS})
    if (aviary_services_loc)
        install (TARGETS aviary_common DESTINATION ${aviary_services_loc}/query)
		install (TARGETS aviary_query DESTINATION ${aviary_services_loc}/query)
    endif()
	file( GLOB JOB_SERVER_SRCS aviary_query_server.cpp Axis2SoapProvider.cpp 
								SubmissionObject.cpp JobServerJobLogConsumer.cpp
								Job.cpp HistoryFile.cpp HistoryProcessingUtils.cpp Globals.cpp )
    condor_exe( aviary_query_server "${JOB_SERVER_SRCS}"
			"${C_SBIN}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${AXIS2_LIBS}" OFF )

endif(LINUX)
