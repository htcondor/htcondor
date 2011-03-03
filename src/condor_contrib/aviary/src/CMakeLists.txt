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

    if ( NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../include" )
        message( FATAL_ERROR "run ../gen-build-xsd-lib.sh first!")
    endif ()

    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})

    # hardcode axis2c headers dir for now
    include_directories( $ENV{WSFCPP_HOME}/include/axis2-1.6.0/ $ENV{WSFCPP_HOME}/include/axis2-1.6.0/platforms )
    include_directories( ${CMAKE_CURRENT_SOURCE_DIR}/../include )
    condor_glob( Hdrs Srcs "${RmvSrcs}" )

    set ( QUERY_SRCS AviaryQueryService.cpp;AviaryQueryServiceMacro.cpp;AviaryQueryServiceSkeleton.cpp;Axis2SoapProvider.cpp)
    set ( QUERY_LIBS ${CMAKE_CURRENT_SOURCE_DIR}/../lib/libaviary_query_types.so )
    set ( AXIS2_LIBS
            $ENV{WSFCPP_HOME}/lib/libaxutil.so $ENV{WSFCPP_HOME}/lib/libaxis2_engine.so 
                $ENV{WSFCPP_HOME}/lib/libaxis2_http_common.so $ENV{WSFCPP_HOME}/lib/libaxis2_parser.so
                    $ENV{WSFCPP_HOME}/lib/libaxis2_http_receiver.so )
    
    dprint( "QUERY_SRCS = ${QUERY_SRCS}" )

    # leave codegen out for now
#
#    add_custom_command( TARGET aviaryquery
#                        PRE_BUILD
#                        COMMAND WSDL2CPP.sh
#                        ARGS -u ${CMAKE_CURRENT_SOURCE_DIR}/../etc/aviary-query.wsdl -o ../codegen/query -d adb )

    dprint( "RmvSrcs = ${RmvSrcs}" )
    dprint( "Srcs = ${Srcs}" )
    dprint( "Hdrs = ${Hdrs}" )

    condor_static_lib( aviaryquery "${Hdrs};${Srcs};${WS02_GEN_SRCS}" )
    condor_exe( aviary_query_server "aviary_query_server.cpp" "${C_SBIN}" "aviaryquery;${AXIS2_LIBS};${QUERY_LIBS};${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND}" OFF )

    condor_static_lib( aviary_utils "AviaryUtils.h;AviaryUtils.cpp" )

    # used for selective compilation differences only otherwise just link the library
    src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
    src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

    # NOTE: Still need to update linkage deps
    file( GLOB SchedSrcs SchedulerObject.* SubmitterObject.* SubmitterUtils.* PROC_ID_comparator.* SubmissionObject.* AviaryMgmtScheddPlugin* AviaryUtils.* )
    condor_plugin( AviaryScheddPlugin-plugin "${COLLECTOR_REF};${UTILS_REF};${SCHED_SRCS}" "${C_LIBEXEC}" "${AXIS2_LIBS}" ON)

endif(LINUX)
