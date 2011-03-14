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

if(WINDOWS)

	# In 2.8 there are issues with including .NET projects, so sadly I have decided to prebuild :-( 
	# path exists in their repo and 2.8.1 we can fix. 
	#include_external_msproject(condor_mail ${CONDOR_SOURCE_DIR}/src/condor_mail/condor_mail.vcproj )
	# Because CMake does not support generating .NET projects in general, condor_mail.exe was prebuilt
	# and dumped into msconfig.  If we ever decide to convert it into a native app, we can enable an
	# actual project here.  For the time being we will keep the condor_mail.vcproj around for backup
	# purposes, also in msconfig, in case we ever find a need to change condor_mail's code.
	install(FILES ${C_WIN_BIN}/condor_mail.exe
			DESTINATION "${C_BIN}")

endif(WINDOWS)
