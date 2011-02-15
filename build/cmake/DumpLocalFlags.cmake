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

MACRO ( DUMP_LOCAL_FLAGS _DUMP_VAR )

	get_directory_property(CDEFS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} COMPILE_DEFINITIONS)
	get_directory_property(IDEFS DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} INCLUDE_DIRECTORIES)

	#loop through -D Vars
	foreach ( _INDEX ${CDEFS} )

		if(${_DUMP_VAR})
			set (${_DUMP_VAR} "${${_DUMP_VAR}};-D${_INDEX}")
		else(${_DUMP_VAR})
			set (${_DUMP_VAR} -D${_INDEX})
		endif(${_DUMP_VAR})

	endforeach (_INDEX)

	# loop through -I Vars
	foreach ( _INDEX ${IDEFS} )

		if(${_DUMP_VAR})
			set (${_DUMP_VAR} "${${_DUMP_VAR}};-I${_INDEX}")
		else(${_DUMP_VAR})
			set (${_DUMP_VAR} -I${_INDEX})
		endif(${_DUMP_VAR})

	endforeach (_INDEX)

ENDMACRO ( DUMP_LOCAL_FLAGS )
