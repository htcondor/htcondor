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


include_directories(${CMAKE_CURRENT_BINARY_DIR})

## create targets
set( collectorElements
collector_main.cpp
offline_plugin.cpp
)


set(CollectorLibSrcs
	CollectorPluginManager.cpp
	collector_stats.cpp
	collector_engine.cpp
	view_server.cpp
	collector.cpp
)

condor_daemon ( EXE condor_collector
  SOURCES "${collectorElements};${CollectorLibSrcs}"
  LIBRARIES "${CONDOR_LIBS};${CONDOR_QMF}"
  INSTALL ${C_SBIN} )

if (LINUX)
    # Linux doesn't require a library's libraries to be on the link line,
    # and none of the other invocations of condor_plugin() use the library
    # argument -- although they're all explicitly Linux-specific.  For now,
    # only pass the libraries in on Windows and Mac.
    condor_plugin( ce-audit "ce-audit-plugin.cpp" "${C_LIBEXEC}" condor_utils off )
else()
    # Collector plug-ins can't be be built on Windows or Mac because the
    # CollectorPlugin constructor and destructor are implemented in the
    # collector binary, not in a library we can link against.  If anybody
    # ever actually cares, we can probably change that.
    #
    # Moving the CollectorPlugin constructor to a library would probably
    # also require moving the plugin manager to a library, since they
    # need to be able to find each other.
    # condor_plugin( ce-audit "ce-audit-plugin.cpp" "${C_LIBEXEC}" "${CONDOR_LIBS}" off )
endif()
