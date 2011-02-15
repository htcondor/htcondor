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

MACRO ( STDU_EXTRACT_AND_TOUPPER _LIB _OBJ_FILE _SYMBOL_LIST  )

	# extract from the library.
	set (_OBJCOPY_COMMAND objcopy)

	foreach ( _ITEM ${_SYMBOL_LIST} )
		string( TOUPPER "${_ITEM}" NEW_UPPER_ITEM_NAME )
		set (_OBJCOPY_COMMAND ${_OBJCOPY_COMMAND} --redefine-sym ${_ITEM}=${NEW_UPPER_ITEM_NAME})
	endforeach(_ITEM)

	# with one command ar extract && objcopy redefine symbols ToUpper.
	add_custom_command( OUTPUT ${_OBJ_FILE}
				COMMAND ar
				ARGS -x ${_LIB} ${_OBJ_FILE} && ${_OBJCOPY_COMMAND} ${_OBJ_FILE} )

	#dprint("Symbol extraction = ar -x ${_LIB} ${_OBJ_FILE} && ${_OBJCOPY_COMMAND} ${_OBJ_FILE}")
	append_var(STDU_OBJS ${_OBJ_FILE})

ENDMACRO ( STDU_EXTRACT_AND_TOUPPER )
