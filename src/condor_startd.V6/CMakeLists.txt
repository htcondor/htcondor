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


if (DARWIN)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework IOKit -framework Carbon")
endif()

if (NOT HAVE_BOINC)
	file( GLOB BOINC_GOO boinc* )
endif()

file( GLOB startdRmvElements Example* *.t.* glexec_wrapper* ${BOINC_GOO} )

condor_daemon( startd "${startdRmvElements}" "${CONDOR_LIBS};${PCRE_FOUND};${OPENSSL_FOUND};${COREDUMPER_FOUND};${KRB5_FOUND};${GLOBUS_FOUND};${CONDOR_QMF}" "${C_SBIN}" OFF)

if (LINUX AND GLOBUS_FOUND)
  condor_exe(condor_glexec_wrapper "glexec_wrapper.cpp" ${C_LIBEXEC} "${CONDOR_TOOL_LIBS};${KRB5_FOUND};${OPENSSL_FOUND};${PCRE_FOUND}" OFF )
  install (FILES glexec_starter_setup.sh DESTINATION ${C_LIBEXEC} PERMISSIONS ${CONDOR_SCRIPT_PERMS} )
endif()
