 ###############################################################
 # 
 # Copyright (C) 2014, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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

# Given the full path to a shared library file, return the SO name of
# that library. For example, the SO name of "/usr/lib64/libssl.so" is
# "libssl.so.10" in the current release of Red Hat 6.
# The result is indicated to be used with dlopen().
# Only supported on Linux.

macro( find_so_name _VAR_OUT _LIB_NAME )

	if ( NOT CMAKE_SYSTEM_NAME MATCHES "Linux" )
		message( FATAL_ERROR "find_so_name() only supported on Linux" )
	endif()

	# objdump -p <lib file> | grep SONAME | sed 's/ *SONAME *//'
	execute_process(COMMAND objdump -p ${_LIB_NAME}
		COMMAND grep SONAME
		COMMAND sed "s/ *SONAME *//"
		OUTPUT_VARIABLE _CMD_OUT
		RESULT_VARIABLE _CMD_RESULT
		OUTPUT_STRIP_TRAILING_WHITESPACE
		)

	if ( _CMD_RESULT EQUAL 0 )
		set( ${_VAR_OUT} ${_CMD_OUT} )
	else()
		message( FATAL_ERROR "Failed to find SO name of ${_LIB_NAME}: ${_CMD_RESULT}" )
	endif()

endmacro()
