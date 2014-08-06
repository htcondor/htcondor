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

MACRO (APPEND_VAR _VAR _VAL)

	if(${_VAR})
		set (${_VAR} "${${_VAR}};${_VAL}")
	else(${_VAR})
		set (${_VAR} ${_VAL})
	endif(${_VAR})

	set (${_VAR} ${${_VAR}} PARENT_SCOPE )

ENDMACRO(APPEND_VAR)


macro(append_target_property_flag _target _property _flag)
    get_property(_val TARGET ${_target} PROPERTY ${_property})
    set(_val "${_val} ${_flag}")
    set_target_properties(${_target} PROPERTIES ${_property} ${_val})
endmacro(append_target_property_flag)
