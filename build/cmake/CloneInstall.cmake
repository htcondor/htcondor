 ###############################################################
 # 
 # Copyright (C) 1990-2010, Redhat. 
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

# CLONE_INSTALL adds code that calls ln_or_cp. We need to make
# sure ln_or_cp is present, so call this once (and only once)
# in any file that calls CLONE_INSTALL.
MACRO (PREP_CLONE_INSTALL)
	install(CODE
		"macro(ln_or_cp src dstdir dstname)
			if( WINDOWS )
				file(INSTALL \${src} DESTINATION \${dstdir} USE_SOURCE_PERMISSIONS RENAME \${dstname})
			else()
				find_program(LN ln)
				if(\${LN} STREQUAL \"LN-NOTFOUND\")
					message(STATUS \"Linking \${src} to \${dstdir}/\${dstname}\")
					execute_process(COMMAND \${LN} \${src} \${dstdir}/\${dstname})
				else()
					message(FATAL_ERROR \"Unable to find ln. Cannot hard link \${src} to \${dstdir}/\${dstname}\")
				endif()
			endif()
		endmacro()")
ENDMACRO()

MACRO (CLONE_INSTALL _ORIG_TARGET _NEWNAMES _INSTALL_LOC )

	if ( WINDOWS )
		set(WIN_EXE_EXT .exe)
		set(${_ORIG_TARGET}_loc ${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${_ORIG_TARGET}${WIN_EXE_EXT})
	else(WINDOWS)
		get_target_property(${_ORIG_TARGET}_loc ${_ORIG_TARGET} LOCATION )
	endif( WINDOWS )

	foreach ( new_target ${_NEWNAMES} )
		install (CODE "ln_or_cp(\"${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}/${_ORIG_TARGET}\" \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}\" \"${new_target}${WIN_EXE_EXT}\")")

	endforeach(new_target)

ENDMACRO (CLONE_INSTALL)
