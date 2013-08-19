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

##########################################################################
# Set project
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

##########################################################################
#Setup package meta-data (used in packaging)
set(PACKAGE "condor")

# Condor version info
#   Condor and other systems parse this number. Keep it simple:
#   Number.Number.Number. Do nothing else.  If you need to add
#   more information, PRE_RELEASE is usually the right location.
set(VERSION "8.1.2")

# Set PRE_RELEASE to either a string (i.e. "PRE-RELEASE-UWCS") or OFF
#   This shuld be "PRE-RELEASE-UWCS most of the time, and OFF when
#   building a release candidate.  If you need to identify builds,
#   perhaps because you are working on a feature branch and need to
#   ship test binaries to a user, change this to identify your work.
#   Avoid whitespace.
set(PRE_RELEASE "PRE-RELEASE-UWCS")
#set(PRE_RELEASE OFF)

# If set via the cmake command line, FORCE_PRE_RELEASE allows for
#   PRE_RELEASE to be forced into a build.
#   cmake ... -DFORCE_PRE_RELEASE:STRING=PRE-RELEASE-NMI
if( FORCE_PRE_RELEASE )
  set(PRE_RELEASE ${FORCE_PRE_RELEASE})
endif( FORCE_PRE_RELEASE )

set(PACKAGE_BUGREPORT "http://condor-wiki.cs.wisc.edu/index.cgi/tktnew")
set(PACKAGE_NAME ${PACKAGE} )
set(PACKAGE_VERSION ${VERSION} )
set(PACKAGE_STRING ${PACKAGE} )
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OS_NAME )
string( TOUPPER "${CMAKE_SYSTEM_VERSION}" OS_VER )
string( TOUPPER "${CMAKE_SYSTEM_PROCESSOR}" SYS_ARCH )
site_name(HOSTNAME)
option(_DEBUG "Debug statements and set default build config" OFF)

###########################################
# start cmake module inclusion
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake/macros;${CONDOR_SOURCE_DIR}/build/cmake/modules;${CONDOR_SOURCE_DIR}/build/cmake)

include (dprint) # set debug module 1st to all spewing in other modules if needed
include (AppendVar)
include (CommandTarget)
include (gSOAPGen)
include (CloneInstall)
include (FindMultiple)
include (ExternalProject)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (GetTimeDate)
include (CleanString)
include (PackageDate)
include (SystemSpecificInformations)
include (CondorPackageConfig) # setup packaging variables needed prior to install targets
include (CondorGlob)
include (CondorSelectiveGlob)
include (CondorSetLinkLibs)
include (CondorExe)
include (CondorDaemon)
include (CondorPrecompiledHeaders)
include (CondorStaticLib)
include (CondorPlugin)
include (CheckCXXCompilerFlag)
include (CheckCXXSourceCompiles)
include (AddSubdirectories)
include (CTest)
include (CondorPLTest)
include (CondorExeTest)
include (CondorUnitTest)
include (CondorStdExeTest)
include (SrcTargetRef)
include (DumpLocalFlags)
include (CondorPreExternal)
include (CondorPostExternal)
include (CondorConfigure)     # will initiate the configure
include (TarGz) # Must be after include(CondorPackageConfig)

if ( WINDOWS )
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif ( WINDOWS )
##########################################################################
# we only include the single sub to allow make at two levels.
# root - includes externals
# src - src -> only
# this way clean @ root cleans externals while clean @src cleans just the src.
add_subdirectory (src)

##########################################################################
# the following are existing doc targets
# ideally one would transform the building of the docs to cmake or a script

# logic here to support old nmi builds
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/man")

	if (NOT WANT_MAN_PAGES OR ${LATEX} STREQUAL "LATEX-NOTFOUND" OR ${LATEX2HTML} STREQUAL "LATEX2HTML-NOTFOUND")
		message(STATUS "NOTE: Skipping man-pages")
	else()
		
		message(STATUS "NOTE: Will try building man-pages")
		# NOTE: deps are not configured to detect file changes.
		add_custom_command (OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/man1/condor_on.1
						COMMAND make
						ARGS just-man-pages
						WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc )
						#DEPENDS )

		add_custom_target ( docs ALL DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/man1/condor_on.1 )
		
		install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/ DESTINATION ${C_MAN} )
	endif()

else()
	message(STATUS "NOTE: Will install prebuilt man-pages")
	#Use trailing slash; otherwise, it will be C_MAN/man
	install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/ DESTINATION ${C_MAN} )
endif()

##########################################################################
# Must in included post *install targets
include ( CPack )
