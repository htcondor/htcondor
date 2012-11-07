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

if (WINDOWS)

	condor_exe( condor_credd
				"credd.windows.cpp;credd.windows.h"
				${C_SBIN}
				"${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND}"
				ON )
				
else()

	if ( WANT_FULL_DEPLOYMENT )

		file( GLOB CREDRMVSRCS *_cred* )
		condor_glob( CREDDHDRS CREDDSRCS "${CREDRMVSRCS}" )
		condor_exe( condor_credd
			"${CREDDHDRS};${CREDDSRCS}"
			${C_SBIN}
			"${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND}"
			OFF )
		
	else ()

		message(STATUS "NOTE: *credd* targets will not be built WANT_FULL_DEPLOYMENT=FALSE")

	endif()
	
endif (WINDOWS)
