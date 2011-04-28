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


################################################################
# 
#
#
# The macro defines the following variables:
#   ${_TARGET_VER} - equal to input ${_VER}
#   ${_TARGET}_STAGE - base location where files will be placed
#   ${_TARGET}_INSTALL_LOC - install location which will be used as reference
#   ${_TARGET}_PREBUILT - indicates if it is prebuilt used by condor_post_external
# Example usage:
#   condor_pre_external(DRMAA drmaa-1.6 "include;lib" "include/drmaa.h")

MACRO (CONDOR_PRE_EXTERNAL _TARGET _VER _INSTALL_DIRS _EXISTANCE_CHECK )

	# set variables which will be used throughout
	set ( ${_TARGET}_VER ${_VER} )
	set ( ${_TARGET}_STAGE ${EXTERNAL_STAGE}/${${_TARGET}_VER} )
	set ( ${_TARGET}_INSTALL_LOC ${${_TARGET}_STAGE}/install )
	
	file ( MAKE_DIRECTORY ${${_TARGET}_INSTALL_LOC} )
	foreach ( dir ${_INSTALL_DIRS} )
		if (NOT EXISTS ${${_TARGET}_INSTALL_LOC}/${dir})
			file ( MAKE_DIRECTORY ${${_TARGET}_INSTALL_LOC}/${dir} )
		endif()
	endforeach(dir)

	# Check for existance of 
	#message (STATUS "Checking ${${_TARGET}_INSTALL_LOC}/${_EXISTANCE_CHECK}") 
	if ( EXISTS ${${_TARGET}_INSTALL_LOC}/${_EXISTANCE_CHECK} )
		#message (STATUS "FOUND!") 
		set (${_TARGET}_PREBUILT ON )
	endif()

ENDMACRO (CONDOR_PRE_EXTERNAL)
