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

set (MONGODB_FOUND "-lmongoclient -lboost_system -lboost_filesystem")

############################
# file target references.
############################

file( GLOB ETL_COMMON_SRCS ODSMongodbOps.cpp ODSPoolUtils.cpp)

file( GLOB ETL_SCHEDD_SRCS ${ETL_COMMON_SRCS} PROC_ID_comparator.cpp ODSScheddPlugin*.cpp )

file( GLOB ETL_SERVER_SRCS ods_job_daemon_main.cpp 
					ODSJobLogConsumer.cpp
					ODSHistoryFile.cpp ODSHistoryUtils.cpp 
					${ETL_COMMON_SRCS} )

############################
# daemon emitter plugins
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

file( GLOB SchedSrcs ODSScheddPlugin*.cpp )
#condor_plugin( ODSScheddPlugin "${SchedSrcs}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)

file( GLOB CollectorSrcs ODSCollectorPlugin.cpp ${ETL_COMMON_SRCS})
condor_plugin( ODSCollectorPlugin "${CollectorSrcs}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)

file( GLOB NegotiatorSrcs ODSNegotiatorPlugin.cpp ${ETL_COMMON_SRCS})
condor_plugin( ODSNegotiatorPlugin "${NegotiatorSrcs}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)

file( GLOB MasterSrcs ODSMasterPlugin.cpp ${ETL_COMMON_SRCS})
condor_plugin( ODSMasterPlugin "${MasterSrcs}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)

file( GLOB StartdSrcs ODSStartdPlugin.cpp ${ETL_COMMON_SRCS})
condor_plugin( ODSStartdPlugin "${StartdSrcs}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)
    
############################
# job etl daemon
############################
condor_exe( ods_job_etl_server "${ETL_SERVER_SRCS}"
  			"${C_SBIN}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${MONGODB_FOUND}" OFF )

############################
# history loader tool
############################
set(TOOL_LINK_LIBS "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}")
condor_exe(ods_job_etl_tool "ods_history_tool_main.cpp" ${C_BIN} "${TOOL_LINK_LIBS};${MONGODB_FOUND}" OFF)

############################
# install targets
############################
#install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/62plumage.config DESTINATION "${C_ETC_EXAMPLES}")
