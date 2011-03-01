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



if (HAVE_EXT_GSOAP AND HAVE_EXT_OPENSSL AND HAVE_PTHREADS)

	if (PRE_WSEAPPI_CPP)
	  add_definitions(-DPRE_WSEAPPI_CPP)
	endif(PRE_WSEAPPI_CPP)
	
	set (AMAZON_SOAP_FILES "soapC.cpp;soapClient.cpp;soapH.h;soapStub.h;soapAmazonEC2Object.h;soapAmazonEC2Proxy.h")

	file( GLOB RmvSrcs *ec2* ${AMAZON_SOAP_FILES} soapServer.cpp wsseapi.cpp smdevp.cpp )
	condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

	include_directories(${GSOAP_SHARE}/plugin/)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR})
	include_directories(${CMAKE_CURRENT_BINARY_DIR})

	command_target(gen_amazon_soapfiles ${SOAPCPP2} "-L;-w;-x;-I${GSOAP_SHARE}/import:${CMAKE_CURRENT_SOURCE_DIR};${CMAKE_CURRENT_SOURCE_DIR}/gsoap_ec2.h" "${AMAZON_SOAP_FILES};soapServer.cpp;AmazonEC2.nsmap")

	condor_exe( amazon_gahp
	"${HeaderFiles};${SourceFiles};${AMAZON_SOAP_FILES};${WSSEAPI};${SMDEVP}" ${C_SBIN}
	"${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${GSOAP_FOUND};${ZLIB_FOUND}"
	OFF)

	add_dependencies(gen_amazon_soapfiles gsoap)
	set_source_files_properties (${WSSEAPI} ${SMDEVP} PROPERTIES GENERATED TRUE)

	if (NOT WINDOWS)
      set_source_files_properties( ${WSSEAPI} ${SMDEVP} soapC.cpp soapClient.cpp PROPERTIES COMPILE_FLAGS "-w")
	endif()


	condor_exe_test(ec2_sim "ec2_sim_main.cpp;ec2_sim_impl.cpp;soapC.cpp;soapServer.cpp" "${CONDOR_TOOL_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${GSOAP_FOUND};${ZLIB_FOUND}")

endif()
