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

MACRO (FIND_MULTIPLE _NAMES _VAR_FOUND)

	## Normally find_library will exist on 1st match, for some windows libs 
	## there is one target and multiple libs

	foreach ( loop_var ${_NAMES} )
	
		find_library( ${_VAR_FOUND}_SEARCH_${loop_var} NAMES ${loop_var} )

			if (NOT ${${_VAR_FOUND}_SEARCH_${loop_var}} STREQUAL "${_VAR_FOUND}_SEARCH_${loop_var}-NOTFOUND" )

				if (${_VAR_FOUND})
					list(APPEND ${_VAR_FOUND} ${${_VAR_FOUND}_SEARCH_${loop_var}} )
				else()
					set (${_VAR_FOUND} ${${_VAR_FOUND}_SEARCH_${loop_var}} )
				endif()
			endif()

	endforeach(loop_var)

	if (${_VAR_FOUND})
		set (TEMPLST ${_NAMES})
		list(LENGTH TEMPLST NUM_SEARCH)
		list(LENGTH ${_VAR_FOUND} NUM_FOUND)
		dprint("searching[${NUM_SEARCH}]:(${_NAMES}) ... found[${NUM_FOUND}]:(${${_VAR_FOUND}})")
	else()
		if (SOFT_IS_HARD)
			message(FATAL_ERROR "Could not find libs(${_NAMES})")
		else()
			message(STATUS "Could not find libs(${_NAMES})")
		endif()
	endif()

ENDMACRO (FIND_MULTIPLE)
