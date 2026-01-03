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
      set(KBDD_LIBS "${CONDOR_LIBS};${HAVE_X11};${HAVE_XSS}")
   else()
      set(KBDD_LIBS "${CONDOR_LIBS};${HAVE_X11}")
   endif()

   if (HAVE_DBUS)
      set(KBDD_LIBS "${KBDD_LIBS};-l${DBus1_LIBRARIES}")
      list(APPEND kbdSrcs MutterInterface.unix.cpp)
   endif()

   if (HAVE_LIBSYSTEMD)
      set(KBDD_LIBS "${KBDD_LIBS};-l${LIBSYSTEMD_LIBRARIES}")
   endif()

   condor_exe( condor_kbdd "${kbdHdrs};${kbdSrcs}" ${C_SBIN} "${KBDD_LIBS}" ON )

else()
    message(STATUS "NOTE: *condor_kbdd* will not be built, HAVE_KBDD doesn't exist")
endif()
