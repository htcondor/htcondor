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

MACRO (CONDOR_PLUGIN _CNDR_TARGET _SRCS _INSTALL_LOC _LINK_LIBS _COPY_PDBS )

    add_library(${_CNDR_TARGET} SHARED ${_SRCS})

    if (CONDOR_EXTERNALS)
        add_dependencies ( ${_CNDR_TARGET} ${CONDOR_EXTERNALS} )
    endif()

	target_link_libraries(${_CNDR_TARGET} PRIVATE "${_LINK_LIBS}")

    # install check
    set(${_CNDR_TARGET}_loc ${_INSTALL_LOC})
    if (${_CNDR_TARGET}_loc)
        install (TARGETS ${_CNDR_TARGET} DESTINATION ${_INSTALL_LOC} )
    endif()

    # disable the lib from name
    set_property( TARGET ${_CNDR_TARGET} PROPERTY PREFIX "" )

    # the following will install the .pdb files, some hackery needs to occur because of build configuration is not known till runtime.
    if ( WINDOWS )
        if ( ${_COPY_PDBS} )
            INSTALL(CODE "FILE(INSTALL DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${_INSTALL_LOC}\" TYPE EXECUTABLE FILES \"${CMAKE_CURRENT_BINARY_DIR}/\${CMAKE_INSTALL_CONFIG_NAME}/${_CNDR_TARGET}.pdb\")")
        endif ()

        set_property( TARGET ${_CNDR_TARGET} PROPERTY FOLDER "plugins" )
    endif( WINDOWS )

ENDMACRO(CONDOR_PLUGIN)
