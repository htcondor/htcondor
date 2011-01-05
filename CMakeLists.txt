 ###############################################################
 # 
 # Copyright (C) 1990-2010, Redhat. 
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
cmake_minimum_required(VERSION 2.8)

##########################################################################
#Setup package meta-data (used in packaging)
set(PACKAGE "condor")
set(VERSION "7.5.6")
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
list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/build/cmake)
include (dprint) # set debug module 1st to all spewing in other modules if needed
include (AppendVar)
include (CommandTarget)
include (gSOAPGen)
include (CloneInstall)
include (FindMultiple)
include (ExternalProjectPatched)
include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckStructHasMember)
include (CheckTypeExists)
include (CheckLibraryExists)
include (CheckTypeSize)
include (PackageDate)
include (SystemSpecificInformations)
include (CondorPackageConfig) # setup packaging variables needed prior to install targets
include (CondorGlob)
include (CondorSelectiveGlob)
include (CondorSetLinkLibs)
include (CondorExe)
include (CondorDaemon)
include (CondorStaticLib)
include (CheckCXXCompilerFlag)
include (CheckCXXSourceCompiles)
include (CondorConfigure)     # will initiate the configure
include (AddSubdirectories)
include (CondorPLTest)
include (CondorExeTest)
include (CondorStdExeTest)
include (SrcTargetRef)
include (DumpLocalFlags)

if ( WINDOWS )
	set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif ( WINDOWS )
##########################################################################
# we only include the single sub to allow make at two levels.
# root - includes externals
# src - src -> only
# this way clean @ root cleans externals while clean @src cleans just the src.
add_subdirectory (src)

# if the man pages exist then install them
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/doc/man")
  #Use trailing slash; otherwise, it will be C_MAN/man
  install( DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc/man/ DESTINATION ${C_MAN} )
endif()

##########################################################################
# Must in included post *install targets
include ( CPack )
