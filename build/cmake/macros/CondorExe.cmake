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

MACRO (CONDOR_EXE _CNDR_TARGET _SRCS_PARAM _INSTALL_LOC _LINK_LIBS _COPY_PDBS)

	# ADD_PRECOMPILED_HEADER macro expects to operate on a global _SRCS
	SET(_SRCS ${_SRCS_PARAM})
	ADD_PRECOMPILED_HEADER()

	add_executable( ${_CNDR_TARGET} ${_SRCS})

	condor_set_link_libs( ${_CNDR_TARGET} "${_LINK_LIBS}")

	set(${_CNDR_TARGET}_loc ${_INSTALL_LOC})

	if (${_CNDR_TARGET}_loc)
		install (TARGETS ${_CNDR_TARGET} DESTINATION ${_INSTALL_LOC} )
		#dprint ("${_CNDR_TARGET} install destination (${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC})")
	endif()
	
	# the following will install the .pdb files, some hackery needs to occur because of build configuration is not known till runtime.
	if ( WINDOWS )		

        set( ${_CNDR_TARGET}_pdb ${_COPY_PDBS} )

        if ( ${_CNDR_TARGET}_pdb )
			INSTALL(CODE "FILE(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}\" TYPE EXECUTABLE FILES \"${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${_CNDR_TARGET}.pdb\")")
		endif ()
		
		set_property( TARGET ${_CNDR_TARGET} PROPERTY FOLDER "executables" )

	endif( WINDOWS )

	if ( DARWIN )
		add_custom_command( TARGET ${_CNDR_TARGET} POST_BUILD
			COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs ${_CNDR_TARGET} )
	endif ( DARWIN )

ENDMACRO (CONDOR_EXE)
