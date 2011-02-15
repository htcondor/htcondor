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

macro(add_subdirectories rootdir prio exclude)
 file(GLOB localsub "${rootdir}/*" )

  foreach(_item ${prio})
	message(STATUS "adding directory (${_item})")
	add_subdirectory( ${_item} )
	list(REMOVE_ITEM localsub ${_item})
  endforeach(_item)

  foreach(dir ${localsub})
    if(IS_DIRECTORY ${dir})
        
	foreach (exclude_var ${exclude})
		if ( ${dir} STREQUAL ${exclude_var})
			message(STATUS "excluding directory (${dir})")
        		set( ${dir}_exclude ON )
		endif()
	endforeach(exclude_var)

	if ( EXISTS ${dir}/CMakeLists.txt AND NOT ${dir}_exclude )
		message(STATUS "adding directory (${dir})")
        	add_subdirectory( ${dir} )
	endif()

    endif()
  endforeach(dir)
endmacro()
