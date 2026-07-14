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
    if ( WINDOWS )
        # Add windows version information to the exe 
        # Refer to http://msdn.microsoft.com/en-us/library/aa381058(VS.85).aspx
        # for more info.
        STRING(REGEX REPLACE "^([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1" CONDOR_MAJOR_VERSION "${VERSION}")
        STRING(REGEX REPLACE "^[0-9]+\\.([0-9])+\\.[0-9]+" "\\1" CONDOR_MINOR_VERSION "${VERSION}")
        STRING(REGEX REPLACE "^[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1" CONDOR_BUILD_NUMBER  "${VERSION}")
        set(CONDOR_EXECUTABLE_NAME ${_CNDR_TARGET})

        CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/msconfig/versioninfo.rc.in ${CMAKE_CURRENT_BINARY_DIR}/versioninfo_${CONDOR_EXECUTABLE_NAME}.rc)
        list(APPEND _SRCS ${CMAKE_CURRENT_BINARY_DIR}/versioninfo_${CONDOR_EXECUTABLE_NAME}.rc)
    endif( WINDOWS )

    add_executable( ${_CNDR_TARGET} ${_SRCS})

	# always link in the condor_version.o for CondorVersion to parse
	target_link_libraries(${_CNDR_TARGET} PRIVATE "condor_version_obj;${_LINK_LIBS}")
	add_dependencies(${_CNDR_TARGET} condor_version_obj)

    set(${_CNDR_TARGET}_loc ${_INSTALL_LOC})

    if (${_CNDR_TARGET}_loc)
        install (TARGETS ${_CNDR_TARGET} DESTINATION ${_INSTALL_LOC} )
        #dprint ("${_CNDR_TARGET} install destination (${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC})")
    endif()
    
    # Install the .pdb alongside the exe. The TARGET_PDB_FILE generator
    # expression resolves the config-specific path and pdb name, and OPTIONAL
    # keeps install from failing when a configuration produces no pdb.
    if ( WINDOWS )
        if ( _COPY_PDBS )
            install(FILES $<TARGET_PDB_FILE:${_CNDR_TARGET}>
                    DESTINATION ${_INSTALL_LOC}
                    OPTIONAL)
        endif ()

        set_property( TARGET ${_CNDR_TARGET} PROPERTY FOLDER "executables" )

        #add updated manifest only for VS2012 and above
        if(NOT (MSVC_VERSION LESS 1700))
            add_custom_command( TARGET ${_CNDR_TARGET} POST_BUILD 
                COMMAND mt.exe -nologo /manifest ${CMAKE_SOURCE_DIR}/msconfig/win7.manifest /outputresource:${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${_CNDR_TARGET}.exe)
        endif(NOT (MSVC_VERSION LESS 1700))

    endif( WINDOWS )

    if ( APPLE )
        # Fix up the share library dependencies
        install( CODE "execute_process(COMMAND ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_rewrite_libs \$ENV{DESTDIR}/${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}/${_CNDR_TARGET})" )
    endif ( APPLE )

ENDMACRO (CONDOR_EXE)
