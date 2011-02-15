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

if (NOT WINDOWS)
	condor_glob( edHdrs edSrcs "" )

	add_executable( condor_eventd ${edHdrs} ${edSrcs} )

	condor_set_link_libs( condor_eventd "${PCRE_FOUND};${CONDOR_LIBS};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}")

	dprint("(TSTCLAIR)->ALL why is eventd not installed? Why build at all?")
endif()