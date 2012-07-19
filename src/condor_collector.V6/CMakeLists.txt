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


if (NOT WIN_EXEC_NODE_ONLY)

	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	## create targets
	file( GLOB collectorRmvElements Example* )

	condor_selective_glob( "collector_stats.*;collector_engine.*;view_server.*;collector.*" CollectorLibSrcs)
	condor_static_lib ( collectorlib "${CollectorLibSrcs}")

	condor_daemon ( collector
					"${collectorRmvElements};${CollectorLibSrcs}"
					"${CONDOR_LIBS};${GSOAP_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${ZLIB_FOUND};${CONDOR_QMF};collectorlib"
					${C_SBIN}
					ON )
	if (HAVE_EXT_GSOAP)
		install ( FILES ${CMAKE_CURRENT_BINARY_DIR}/condorCollector.wsdl DESTINATION ${C_LIB}/webservice )
	endif(HAVE_EXT_GSOAP)

	add_subdirectory(tests)

endif(NOT WIN_EXEC_NODE_ONLY)
