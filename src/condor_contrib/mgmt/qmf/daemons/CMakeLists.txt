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

if (LINUX)

    include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR})
    file( GLOB RmvSrcs test* *_main* )
    condor_glob( Hdrs Srcs "${RmvSrcs}" )

    set( QMF_GEN_SRCS Package.h;Package.cpp;ArgsJobServerGetJobAd.h;ArgsJobServerFetchJobData.h;ArgsSubmissionGetJobSummaries.h;ArgsGridMethodsEcho.h;Submission.h;Submission.cpp;JobServer.h;JobServer.cpp)

    # qmf gen & copy out the results for out of source builds
    add_custom_command( OUTPUT ${QMF_GEN_SRCS} 
                        COMMAND python
                        ARGS ${QMF_GEN} ${CMAKE_CURRENT_SOURCE_DIR}/job_server.xml && cd com/redhat/grid && mv -f * ../../.. )

    add_custom_target (qmf_gen_jobserver_srcs ALL DEPENDS ${QMF_GEN_SRCS} )
    add_dependencies( qmf_gen_jobserver_srcs qpid )

    condor_static_lib( jobserver "${Hdrs};${Srcs};${QMF_GEN_SRCS}" ) # just to build all srcs then let the linker sort it out.
    condor_exe( condor_job_server "job_server_main.cpp" "${C_SBIN}" "jobserver;qmf_utils;${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${QPID_FOUND};broker_utils" OFF )
    condor_exe_test(test_history "test_history.cpp" "jobserver;qmf_utils;${CONDOR_TOOL_LIBS}")

endif(LINUX)
