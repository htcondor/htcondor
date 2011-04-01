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

	# shouldn't happen if gen code checked-in
    if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../codegen" )
        message( FATAL_ERROR "run ../gen-build-xsd-lib.sh first!")
    endif ()

	# proper v. WSFCPP_HOME build
    if ( PROPER )

		# hackiness for /usr/lib64 softlink later
		if ( ${SYS_ARCH} STREQUAL "X86_64" )
			set (WS02_LIB_ROOT "/usr/lib64")
		else()
			set (WS02_LIB_ROOT "/usr/lib")
		endif ()

		find_multiple( "axis2_engine;axutil;axis2_axiom;axis2_parser;axis2_http_common;axis2_http_receiver;axis2_http_sender;neethi;guththila;wso2_wsf" WSO2_FOUND )
       message (STATUS "external configured WSO2_FOUND=${WSO2_FOUND}")

       include_directories( /usr/include/axis2-1.6.0
			    /usr/include/wso2-wsf-cpp
			    /usr/include/axis2-1.6.0/platforms )
	    set ( AXIS2_LIBS -L${WS02_LIB_ROOT} ${WSO2_FOUND} )

		set (axis2_repo_loc ${CMAKE_INSTALL_PREFIX})

	else (PROPER)

		# TODO: querying env vars not reliable like this
	    if ( NOT DEFINED ENV{WSFCPP_HOME} )
	        message( FATAL_ERROR "WSFCPP_HOME not defined for non-proper build")
	    endif ()

		set ( AXIS2_LIBS -L$ENV{WSFCPP_HOME}/lib
			 -libaxis2_engine
		     -laxutil -laxis2_axiom -laxis2_parser
		     -lneethi -lguththila
		     -laxis2_http_common -laxis2_http_receiver -laxis2_http_sender
		     -lwso2_wsf )

		include_directories(
			    $ENV{WSFCPP_HOME}/include/axis2-1.6.0
			    $ENV{WSFCPP_HOME}/include/wso2-wsf-cpp
			    $ENV{WSFCPP_HOME}/include/axis2-1.6.0/platforms )

        set(axis2_repo_loc $ENV{WSFCPP_HOME})

	endif ( PROPER )



    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

	#
    # WS02 codegen details
	#
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )

    file( GLOB COMMON_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/common/src/*.cpp )

    file( GLOB JOB_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/job/src/*.cpp )
    file( GLOB JOB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/AviaryJobService*.cpp )

	file( GLOB QUERY_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/query/src/*.cpp )
    file( GLOB QUERY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryQueryService*.cpp )

	set(aviary_services_loc ${axis2_repo_loc}/services)

	#
	# schedd plugin
	#
    # used for selective compilation differences only otherwise just link the library
    src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
    src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

    add_library (aviary_job_axis SHARED ${COMMON_GEN_SRCS} ${JOB_GEN_SRCS} ${JOB_SRCS} Codec.cpp AviaryUtils.cpp SchedulerObject.cpp )

	# remove unresolved deps by copying in -lwso2_wsf objects
	target_link_libraries(aviary_job_axis --whole-archive -lwso2_wsf --no-whole-archive)
    if (aviary_services_loc)
		install (TARGETS aviary_job_axis DESTINATION ${aviary_services_loc}/job)
		install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../services/job/services.xml DESTINATION ${aviary_services_loc}/job )
		install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-common.xsd
						${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-job.xsd
						${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-job.wsdl
					DESTINATION ${aviary_services_loc}/job )
    endif()

    file( GLOB SCHED_PLUGIN_SRCS Codec.cpp AviaryUtils.cpp SchedulerObject.cpp
			     PROC_ID_comparator.cpp Axis2SoapProvider.cpp AviaryScheddPlugin*.cpp )
	condor_plugin( AviaryScheddPlugin-plugin
		"${COLLECTOR_REF};${UTILS_REF};${SCHED_PLUGIN_SRCS}" "${C_LIBEXEC}" "${AXIS2_LIBS}" ON)
	# remove unresolved deps by copying in -laxis2_engine objects
	target_link_libraries(AviaryScheddPlugin-plugin --whole-archive -laxis2_engine --no-whole-archive)
	set_property( TARGET AviaryScheddPlugin-plugin PROPERTY PREFIX "" )

	#
	# query server (aka ye old job server)
	#
	add_library (aviary_query_axis SHARED ${COMMON_GEN_SRCS} ${QUERY_GEN_SRCS} ${QUERY_SRCS} )
	target_link_libraries (aviary_query_axis ${AXIS2_LIBS})

	file( GLOB JOB_SERVER_SRCS aviary_query_server.cpp Axis2SoapProvider.cpp 
								SubmissionObject.cpp JobServerJobLogConsumer.cpp
								Job.cpp HistoryFile.cpp HistoryProcessingUtils.cpp Globals.cpp
								Codec.cpp JobServerObject.cpp AviaryUtils.cpp )
    condor_exe( aviary_query_server "${JOB_SERVER_SRCS}"
			"${C_SBIN}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${AXIS2_LIBS}" OFF )

	install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/axis2.xml DESTINATION ${axis2_repo_loc} )

    if (aviary_services_loc)
		install (TARGETS aviary_query_axis DESTINATION ${aviary_services_loc}/query)
		install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../services/query/services.xml DESTINATION ${aviary_services_loc}/query )
		install (FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-common.xsd
					${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.xsd
					${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.wsdl
				DESTINATION ${aviary_services_loc}/query )
	endif()

	if (PROPER)
		install (CODE "execute_process(COMMAND echo set WSFCPP_HOME to \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC} for local testing)")
	endif (PROPER)

	install(FILES	${CMAKE_CURRENT_SOURCE_DIR}/../etc/61aviary.config
		DESTINATION "${C_ETC_EXAMPLES}")

endif(LINUX)
