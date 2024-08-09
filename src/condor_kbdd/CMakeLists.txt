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

if (HAVE_KBDD)

set(kbdSrcs
kbdd.cpp
)

if (NOT WINDOWS)
	list(APPEND kbdSrcs XInterface.unix.cpp)
endif()

  if (HAVE_XSS)
	condor_exe( condor_kbdd "${kbdHdrs};${kbdSrcs}" ${C_SBIN} "${CONDOR_LIBS};${HAVE_X11};${HAVE_XSS}" ON )
  else()
	condor_exe( condor_kbdd "${kbdHdrs};${kbdSrcs}" ${C_SBIN} "${CONDOR_LIBS};${HAVE_X11}" ON )
  endif()

else()
    message(STATUS "NOTE: *condor_kbdd* will not be built, HAVE_KBDD doesn't exist")
endif()
