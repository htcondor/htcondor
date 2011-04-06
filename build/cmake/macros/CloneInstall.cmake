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

	if ( WINDOWS )
		set(WIN_EXE_EXT .exe)
		set(${_ORIG_TARGET}_loc ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${_ORIG_TARGET}${WIN_EXE_EXT})
	else(WINDOWS)
		get_target_property(${_ORIG_TARGET}_loc ${_ORIG_TARGET} LOCATION )
	endif( WINDOWS )

	foreach ( new_target ${_NEWNAMES} )

        if (WINDOWS OR ${LN} STREQUAL "LN-NOTFOUND")
            install (CODE "FILE(INSTALL \"${${_ORIG_TARGET}_loc}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}\" USE_SOURCE_PERMISSIONS RENAME \"${new_target}${WIN_EXE_EXT}\")")
        else()
            #install (CODE "execute_process(COMMAND cd \${CMAKE_INSTALL_PREFIX} && ${LN} -v -f ${_ORIG_INSTALL}/${_ORIG_TARGET} ${_INSTALL_LOC}/${new_target})")
            # because it's a hardlink absolute paths should not matter.
	    install (CODE "execute_process(COMMAND ${LN} -v -f \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_ORIG_INSTALL}/${_ORIG_TARGET} \$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}/${new_target})")
        endif()

	endforeach(new_target)

ENDMACRO (CLONE_INSTALL)
