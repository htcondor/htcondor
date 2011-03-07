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

    # TODO: querying env vars not reliable like this
    if ( NOT DEFINED ENV{WSFCPP_HOME} )
        message( FATAL_ERROR "must set WSFCPP_HOME var in env!")
    endif ()

    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

    # hardcode axis2c headers dir for now
    include_directories( $ENV{WSFCPP_HOME}/include/axis2-1.6.0/ $ENV{WSFCPP_HOME}/include/axis2-1.6.0/platforms )
    # path to generated headers
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )

    file( GLOB JOB_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/job/src/*.cpp )
    file( GLOB JOB_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/AviaryJobService*.cpp )
    file( GLOB QUERY_GEN_SRCS ${CMAKE_CURRENT_SOURCE_DIR}/../codegen/query/src/*.cpp )
    file( GLOB QUERY_SRCS ${CMAKE_CURRENT_SOURCE_DIR} AviaryQueryService*.cpp )
    set ( AXIS2_LIBS
            $ENV{WSFCPP_HOME}/lib/libaxutil.so $ENV{WSFCPP_HOME}/lib/libaxis2_engine.so 
                $ENV{WSFCPP_HOME}/lib/libaxis2_http_common.so $ENV{WSFCPP_HOME}/lib/libaxis2_parser.so
                    $ENV{WSFCPP_HOME}/lib/libaxis2_http_receiver.so )

    dprint( "JOB_GEN_SRCS = ${JOB_GEN_SRCS}" )
    dprint( "JOB_SRCS = ${JOB_SRCS}" )
    dprint( "QUERY_GEN_SRCS = ${QUERY_GEN_SRCS}" )
    dprint( "QUERY_SRCS = ${QUERY_SRCS}" )

    add_library (aviary_query SHARED ${QUERY_GEN_SRCS} ${QUERY_SRCS} )
    condor_exe( aviary_query_server "aviary_query_server.cpp;Axis2SoapProvider.cpp" "${C_SBIN}" "${AXIS2_LIBS};${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND}" OFF )

    # used for selective compilation differences only otherwise just link the library
    src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
    src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

    add_library (aviary_job SHARED ${JOB_GEN_SRCS} ${JOB_SRCS} )
    file( GLOB SCHED_PLUGIN_SRCS Codec.cpp PROC_ID_comparator.cpp SchedulerObject.cpp Axis2SoapProvider.cpp AviaryMgmtScheddPlugin.cpp )
    condor_plugin( AviaryScheddPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${SCHED_PLUGIN_SRCS}" "${C_LIBEXEC}" "${AXIS2_LIBS}" ON)

endif(LINUX)
