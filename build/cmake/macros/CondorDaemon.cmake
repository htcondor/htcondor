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

FUNCTION(CONDOR_DAEMON _CNDR_TARGET _REMOVE_ELEMENTS _LINK_LIBS _INSTALL_LOC )

	set(${_CNDR_TARGET}_REMOVE_ELEMENTS ${_REMOVE_ELEMENTS})

	condor_glob( ${_CNDR_TARGET}HDRS ${_CNDR_TARGET}SRCS "${${_CNDR_TARGET}_REMOVE_ELEMENTS}" )

    list(APPEND _LINK_LIBS condor_version_obj)

	#Add the executable target.
	condor_exe( condor_${_CNDR_TARGET} "${${_CNDR_TARGET}HDRS};${${_CNDR_TARGET}SRCS}" ${_INSTALL_LOC} "${_LINK_LIBS}" ON)
	add_dependencies(condor_${_CNDR_TARGET} condor_version_obj)

        # full relro and PIE for daemons/setuid/setgid applications
        if (cxx_full_relro_and_pie)
            # full relro:
            append_target_property_flag(condor_${_CNDR_TARGET} LINK_FLAGS ${cxx_full_relro_arg})
            # PIE:
            append_target_property_flag(condor_${_CNDR_TARGET} COMPILE_FLAGS "-fPIE -DPIE")
            append_target_property_flag(condor_${_CNDR_TARGET} LINK_FLAGS "-pie")
        endif()
	
ENDFUNCTION (CONDOR_DAEMON)
