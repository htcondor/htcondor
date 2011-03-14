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

# condor_glob - will append sources and headers given the
#               current path to input VARS.
#
# IN: 
#	CONDOR_REMOVE		- Elements to remove for whatever reason.
#
# IN-OUT:
#	CONDOR_HEADERS		-.h files for current path excluding remove items
#	CONDOR_SRCS			-.cpp files for current path excluding remove items 
#
MACRO (CONDOR_GLOB CONDOR_HEADERS CONDOR_SRCS CONDOR_REMOVE)

	string(TOUPPER "${CMAKE_SYSTEM_NAME}" TEMP_OS)

	file(GLOB ${CONDOR_HEADERS} "*.h" "*.hpp" "*.hxx")
	file(GLOB ${CONDOR_SRCS} "*.cpp" "*.cxx" "*.c")

	file(GLOB WinSrcs "*WINDOWS*" "*windows*")
	file(GLOB LinuxSrcs "*LINUX*" "*linux*")
	file(GLOB UnixSrcs "*UNIX*" "*unix*")
	file(GLOB RmvSrcs "*.dead*" "*.o" "*.lo" "*.a" "*.la" "*.dll" "*.lib" ".orig" ${CONDOR_REMOVE})
	#TBD: do we build for any non-posix besides windows?
	# they appear to be ignored in the imake files.

	# 1st remove any elements that are extranious to the target in question.
	if(RmvSrcs)
		list(REMOVE_ITEM ${CONDOR_SRCS} ${RmvSrcs})
		list(REMOVE_ITEM ${CONDOR_HEADERS} ${RmvSrcs})
	endif()

	if(${TEMP_OS} STREQUAL "LINUX")
		if (WinSrcs)
			list(REMOVE_ITEM ${CONDOR_SRCS} ${WinSrcs})
			list(REMOVE_ITEM ${CONDOR_HEADERS} ${WinSrcs})
		endif()
		# linux vers *will build *.unix.* as was done before
	elseif (${TEMP_OS} STREQUAL "WINDOWS")
		if (LinuxSrcs)
			list(REMOVE_ITEM ${CONDOR_SRCS} ${LinuxSrcs})
			list(REMOVE_ITEM ${CONDOR_HEADERS} ${LinuxSrcs})
		endif()
		if (UnixSrcs)
			list(REMOVE_ITEM ${CONDOR_SRCS} ${UnixSrcs})
			list(REMOVE_ITEM ${CONDOR_HEADERS} ${UnixSrcs})
		endif()
	else()
		if (WinSrcs)
			list(REMOVE_ITEM ${CONDOR_SRCS} ${WinSrcs})
			list(REMOVE_ITEM ${CONDOR_HEADERS} ${WinSrcs})
		endif()
		if (LinuxSrcs)
			list(REMOVE_ITEM ${CONDOR_SRCS} ${LinuxSrcs})
			list(REMOVE_ITEM ${CONDOR_HEADERS} ${LinuxSrcs})
		endif()
	endif()

ENDMACRO (CONDOR_GLOB)
