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

MACRO (CONDOR_SHARED_LIB _CNDR_TARGET)

SET(_SRCS ${ARGN})

# ADD_PRECOMPILED_HEADER macro expects to operate on a global _SRCS
ADD_PRECOMPILED_HEADER()

if ( CONDOR_BUILD_SHARED_LIBS )
	add_library(${_CNDR_TARGET} SHARED ${_SRCS})
	install(TARGETS ${_CNDR_TARGET} DESTINATION ${C_LIB})
else()
	add_library(${_CNDR_TARGET} STATIC ${_SRCS})
endif()

if (CONDOR_EXTERNALS)
	add_dependencies ( ${_CNDR_TARGET} ${CONDOR_EXTERNALS} )
endif()

if ( WINDOWS )
	set_property( TARGET ${_CNDR_TARGET} PROPERTY FOLDER "libraries" )
endif ( WINDOWS )

ENDMACRO(CONDOR_SHARED_LIB)

MACRO (CONDOR_STATIC_LIB _CNDR_TARGET)

SET(_SRCS ${ARGN})

# ADD_PRECOMPILED_HEADER macro expects to operate on a global _SRCS
ADD_PRECOMPILED_HEADER()


add_library(${_CNDR_TARGET} STATIC ${_SRCS})
set_target_properties(${_CNDR_TARGET} PROPERTIES COMPILE_DEFINITIONS "CONDOR_STATIC_LIBRARY")

if (CONDOR_EXTERNALS)
	add_dependencies ( ${_CNDR_TARGET} ${CONDOR_EXTERNALS} )
endif()

if ( WINDOWS )
	set_property( TARGET ${_CNDR_TARGET} PROPERTY FOLDER "libraries" )
endif ( WINDOWS )

ENDMACRO(CONDOR_STATIC_LIB)
