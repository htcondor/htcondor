 ###############################################################
 # 
 # Copyright 2011 Red Hat, Inc. 
 # Copyright (C) 1990-2010, Condor Team, Computer Sciences Department,
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


MACRO (CLONE_INSTALL _ORIG_TARGET _ORIG_INSTALL _NEWNAMES _INSTALL_LOC )
if (WINDOWS)

	set(WIN_EXE_EXT .exe)
	set(${_ORIG_TARGET}_loc ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${_ORIG_TARGET}${WIN_EXE_EXT})
	foreach ( new_target ${_NEWNAMES} )
		install (CODE "FILE(INSTALL \"${${_ORIG_TARGET}_loc}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}\" USE_SOURCE_PERMISSIONS RENAME \"${new_target}${WIN_EXE_EXT}\")")
	endforeach(new_target)

else()

	foreach ( new_target ${_NEWNAMES} )
		# If the link target is not in the same directory as the link source
		# we need to make the link a relative path to the target.  Packaging
		# tools complain if we use absolute paths.
		# file(RELATIVE_PATH computes the relative path from one dir to the other
		file(RELATIVE_PATH PATH_DIFF "${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}" "${CMAKE_INSTALL_PREFIX}/${_ORIG_INSTALL}")

		# If the orig and target dir are the same, file(RELATIVE_PATH returns empty string
		if ("${PATH_DIFF}" STREQUAL "")
			set(PATH_DIFF ".")
		endif()
		install (CODE "execute_process(COMMAND \"${CMAKE_COMMAND}\" -E create_symlink \"${PATH_DIFF}/${_ORIG_TARGET}\" \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}/${new_target})")
	endforeach(new_target)
endif(WINDOWS)

ENDMACRO (CLONE_INSTALL)

