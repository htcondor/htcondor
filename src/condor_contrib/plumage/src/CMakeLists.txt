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


############################
# file target references.
############################
file( GLOB ETL_PLUGIN_SRCS Codec.cpp AviaryUtils.cpp SchedulerObject.cpp
			PROC_ID_comparator.cpp AviaryScheddPlugin*.cpp )

file( GLOB ETL_SERVER_SRCS ods_job_etl_main.cpp 
					ODSJobLogConsumer.cpp
					ODSHistoryFile.cpp ODSHistoryUtils.cpp 
					ODSReaderWriter.cpp )

############################
# emitter plugin
############################
src_target_ref( condor_collector "hashkey" COLLECTOR_REF )
src_target_ref( utils "set_user_priv_from_ad" UTILS_REF )

condor_plugin( ODSEmitterPlugin "${COLLECTOR_REF};${UTILS_REF};${ETL_PLUGIN_SRCS}" "${C_LIBEXEC}" "${MONGODB_FOUND}" ON)

############################
# etl server
############################
condor_exe( ods_job_etl "${ETL_SERVER_SRCS}"
			"${C_SBIN}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${GCB_FOUND};${MONGODB_FOUND}" OFF )

############################
# install targets
############################
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../etc/62plumage.config DESTINATION "${C_ETC_EXAMPLES}")
