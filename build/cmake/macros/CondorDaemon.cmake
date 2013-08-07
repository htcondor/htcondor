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

MACRO (CONDOR_DAEMON _CNDR_TARGET _REMOVE_ELEMENTS _LINK_LIBS _INSTALL_LOC _GEN_GSOAP )

	set(${_CNDR_TARGET}SOAP ${_GEN_GSOAP})
	set(${_CNDR_TARGET}_REMOVE_ELEMENTS ${_REMOVE_ELEMENTS})

    if (NOT ${_CNDR_TARGET}SOAP OR NOT HAVE_EXT_GSOAP )
		file ( GLOB ${_CNDR_TARGET}_REMOVE_ELEMENTS *soap* ${_REMOVE_ELEMENTS} )
    endif()

	condor_glob( ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS "${${_CNDR_TARGET}_REMOVE_ELEMENTS}" )

    if ( ${_CNDR_TARGET}SOAP AND HAVE_EXT_GSOAP )
		gsoap_gen( ${_CNDR_TARGET} ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS )
		list(APPEND ${_CNDR_TARGET}SRCS ${DAEMON_CORE}/soap_core.cpp ${DAEMON_CORE}/mimetypes.cpp)
		list(APPEND ${_CNDR_TARGET}HDRS ${DAEMON_CORE}/soap_core.h ${DAEMON_CORE}/mimetypes.h)
	endif()
	if ( CONDOR_BUILD_SHARED_LIBS )
		list(APPEND ${_CNDR_TARGET}SRCS ${CMAKE_SOURCE_DIR}/src/condor_utils/condor_version.cpp)
	endif()

	#Add the executable target.
	condor_exe( condor_${_CNDR_TARGET} "${${_CNDR_TARGET}HDRS};${${_CNDR_TARGET}SRCS}" ${_INSTALL_LOC} "${_LINK_LIBS}" ON)

	# update the dependencies based on options
	if ( ${_CNDR_TARGET}SOAP AND HAVE_EXT_GSOAP)
		add_dependencies(condor_${_CNDR_TARGET} gen_${_CNDR_TARGET}_soapfiles)
		# Do not export gsoap symbols - they may break loadable modules.
		if ( LINUX )
			if ( PROPER )
				set_target_properties( condor_${_CNDR_TARGET} PROPERTIES LINK_FLAGS "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../condor_daemon_core.V6/daemon.version.proper")
			else()
				set_target_properties( condor_${_CNDR_TARGET} PROPERTIES LINK_FLAGS "-Wl,--exclude-libs=libgsoapssl++.a -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/../condor_daemon_core.V6/daemon.version")
			endif()
		endif()
	endif()
	
ENDMACRO (CONDOR_DAEMON)
