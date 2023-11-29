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

# example call
# Note that all argument values are single arguments strings, perhaps containing ; separated lists
# condor_daemon(EXE condor_schedd SOURCES foo.cpp;bar.cpp LIBRARIES libfoo INSTALL "$(C_SBIN"))

FUNCTION(CONDOR_DAEMON)

	# Define the positional args

	# EXE, INSTALL are named value arguments with just one value
	set(oneValueArgs EXE INSTALL)	

	# SOURCES is the named value argument with many values
	set(multiValueArgs SOURCES LIBRARIES)
	cmake_parse_arguments(CONDOR_DAEMON "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN} )

	# Should check ${CONDOR_DAEMON_UNPARSED_ARGS} for anything

	#Add the executable target.
	if (BUILD_DAEMONS) 
		condor_exe( "${CONDOR_DAEMON_EXE}" "${CONDOR_DAEMON_SOURCES}" "${CONDOR_DAEMON_INSTALL}" "${CONDOR_DAEMON_LIBRARIES}" ON)

		# full relro and PIE for daemons/setuid/setgid applications
		if (cxx_full_relro_and_pie)
			# full relro:
			append_target_property_flag(${CONDOR_DAEMON_EXE} LINK_FLAGS ${cxx_full_relro_arg})
			# PIE:
			append_target_property_flag(${CONDOR_DAEMON_EXE} COMPILE_FLAGS "-fPIE -DPIE")
			append_target_property_flag(${CONDOR_DAEMON_EXE} LINK_FLAGS "-pie")
		endif()
	endif()
	
ENDFUNCTION (CONDOR_DAEMON)
