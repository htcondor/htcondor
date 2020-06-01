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

if (WITH_GANGLIA)

  set(GANGLIAD
	gangliad.cpp
	gangliad_main.cpp
	ganglia_interaction.cpp
	statsd.cpp
  )

  condor_exe( condor_gangliad "${GANGLIAD}" ${C_LIBEXEC} "${CONDOR_LIBS}" OFF )

  install ( FILES ganglia_default_metrics DESTINATION "${C_GANGLIAD}" RENAME 00_default_metrics)

endif()

