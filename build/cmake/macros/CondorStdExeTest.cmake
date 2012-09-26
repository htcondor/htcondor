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

MACRO (CONDOR_STD_EXE_TEST _CNDR_TARGET _COMPILER _SRCS _LINK_FLAGS )

	if (STD_UNIVERSE AND BUILD_TESTING)

		# so normally you should be able to pass the archive directly to 
		foreach(_file ${_SRCS})
			if(objs_${_CNDR_TARGET})
				set(objs_${_CNDR_TARGET} ${objs_${_CNDR_TARGET}};${_file}.o)
			else()
				set(objs_${_CNDR_TARGET} ${_file}.o)
			endif()
		endforeach()

		#dprint("SRCS=${_SRCS}  objs_${_CNDR_TARGET}=${objs_${_CNDR_TARGET}}")
		# you *should* be able to pass a library to std:u
		add_library( ${_CNDR_TARGET} STATIC EXCLUDE_FROM_ALL ${_SRCS})
		add_dependencies( ${_CNDR_TARGET} stdulib )

		#here is where cmake is not so good you can not easily access the object files :-( so rip them out
		command_target( arx_${_CNDR_TARGET} ar "-x;lib${_CNDR_TARGET}.a" "${objs_${_CNDR_TARGET}}")
		add_dependencies( arx_${_CNDR_TARGET} ${_CNDR_TARGET} ${_SRCS})

		# call condor compile, you will need to verify that make install has passed.
		command_target( cc_${_CNDR_TARGET} ${CONDOR_COMPILE} "-condor_lib;${STDU_LIB_LOC};-condor_ld_dir;${STDU_LIB_LOC};${_COMPILER};-o;${CMAKE_CURRENT_BINARY_DIR}/${_CNDR_TARGET}.cndr.exe;${objs_${_CNDR_TARGET}};${_LINK_FLAGS}" "${CMAKE_CURRENT_BINARY_DIR}/${_CNDR_TARGET}.cndr.exe" )
		add_dependencies( cc_${_CNDR_TARGET} arx_${_CNDR_TARGET} ${_SRCS})

		command_target( ca_${_CNDR_TARGET} ${CONDOR_ARCH_LINK} "${_CNDR_TARGET}.cndr.exe" "${CMAKE_CURRENT_BINARY_DIR}/${_CNDR_TARGET}.cndr.exe.LINUX.INTEL")
		add_dependencies( ca_${_CNDR_TARGET} cc_${_CNDR_TARGET} ${_SRCS})

		append_var(CONDOR_TESTS ca_${_CNDR_TARGET})
		set_target_properties( cc_${_CNDR_TARGET} ca_${_CNDR_TARGET} arx_${_CNDR_TARGET} PROPERTIES EXCLUDE_FROM_ALL TRUE)

	endif()

ENDMACRO(CONDOR_STD_EXE_TEST)
