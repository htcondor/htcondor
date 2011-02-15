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

MACRO (CONDOR_SELECTIVE_GLOB _LIST _SRCS)
	string(TOUPPER "${CMAKE_SYSTEM_NAME}" TEMP_OS)

	file(GLOB WinSrcs "*WINDOWS*" "*windows*")
	file(GLOB LinuxSrcs "*LINUX*" "*linux*")
	file(GLOB UnixSrcs "*UNIX*" "*unix*")
	file(GLOB RmvSrcs "*.dead*" "*NON_POSIX*" "*.o" "*.lo" "*.a" "*.la" "*.dll" "*.lib")

	foreach ( _ITEM ${_LIST} )

		# clear the temp var
		set(_TEMP OFF)
		file ( GLOB _TEMP ${_ITEM} )

		if (_TEMP)
			# brute force but it works, and cmake is a one time pad
			if(RmvSrcs)
				list(REMOVE_ITEM _TEMP ${RmvSrcs})
			endif()

			if(${TEMP_OS} STREQUAL "LINUX")
				if (WinSrcs)
					list(REMOVE_ITEM _TEMP ${WinSrcs})
				endif()
				# linux vers *will build *.unix.* as was done before
			elseif (${TEMP_OS} STREQUAL "WINDOWS")
				if (LinuxSrcs)
					list(REMOVE_ITEM _TEMP ${LinuxSrcs})
				endif()
				if (UnixSrcs)
					list(REMOVE_ITEM _TEMP ${UnixSrcs})
				endif()
			else()
				if (WinSrcs)
					list(REMOVE_ITEM _TEMP ${WinSrcs})
				endif()
				if (LinuxSrcs)
					list(REMOVE_ITEM _TEMP ${LinuxSrcs})
				endif()
			endif()

			# now tack on the items.
			list(APPEND ${_SRCS} ${_TEMP} )

		endif(_TEMP)
	endforeach(_ITEM)

	list (REMOVE_DUPLICATES ${_SRCS})

ENDMACRO (CONDOR_SELECTIVE_GLOB)
