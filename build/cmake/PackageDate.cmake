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

# Generate currente date for using in each type of package
# May need to change to add_custom_target instead so that date is captured at build time

MACRO (PACKAGEDATE _TYPE _VAR )	

	if ( ${_TYPE} STREQUAL  "RPM" )
		execute_process(
			COMMAND date "+%a %b %d %Y"			
			OUTPUT_VARIABLE ${_VAR}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			)
	elseif ( ${_TYPE} STREQUAL  "DEB" )
		execute_process(
			COMMAND date -R			
			OUTPUT_VARIABLE ${_VAR}
			OUTPUT_STRIP_TRAILING_WHITESPACE
			)
	endif()

	#message (STATUS ${TYPE} ${${_VAR}})

ENDMACRO (PACKAGEDATE)
