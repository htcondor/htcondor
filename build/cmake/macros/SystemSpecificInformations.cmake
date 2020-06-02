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

# Based on the script with the same name by Eric Noulard 

# define a set of string with may-be useful readable name
# this file is meant to be included in a CMakeLists.txt
# not as a standalone CMake script
set(SPECIFIC_COMPILER_NAME "")
set(SPECIFIC_SYSTEM_VERSION_NAME "")
set(LINUX_NAME "Unknown")

#For Linux, the folowing varaible will be set
#LINUX_NAME		= Fedora, CentOS, RedHat, Ubuntu, openSUSE
#LINUX_VER		= 12 (Fedora), 5.0 (Debian, CentOS)
#RPM_SYSTEM_NAME	= fc12, rhel5, ..
#BIT_MODE		= 32 | 64
#CPACK_DEBIAN_PACKAGE_ARCHITECTURE	= i386, amd64, ...  (value from dpkg utility) 

if(WIN32)
# information taken from
# http://www.codeguru.com/cpp/w-p/system/systeminformation/article.php/c8973/
	# Win9x series
	if(CMAKE_SYSTEM_VERSION MATCHES "4.0")
      set(SPECIFIC_SYSTEM_VERSION_NAME "Win95")
	endif(CMAKE_SYSTEM_VERSION MATCHES "4.0")
	if(CMAKE_SYSTEM_VERSION MATCHES "4.10")
      set(SPECIFIC_SYSTEM_VERSION_NAME "Win98")
	endif(CMAKE_SYSTEM_VERSION MATCHES "4.10")
	if(CMAKE_SYSTEM_VERSION MATCHES "4.90")
      set(SPECIFIC_SYSTEM_VERSION_NAME "WinME")
	endif(CMAKE_SYSTEM_VERSION MATCHES "4.90")

	# WinNTyyy series
	if(CMAKE_SYSTEM_VERSION MATCHES "3.0")
      set(SPECIFIC_SYSTEM_VERSION_NAME "WinNT351")
	endif(CMAKE_SYSTEM_VERSION MATCHES "3.0")
	if(CMAKE_SYSTEM_VERSION MATCHES "4.1")
      set(SPECIFIC_SYSTEM_VERSION_NAME "WinNT4")
	endif(CMAKE_SYSTEM_VERSION MATCHES "4.1")

    # Win2000/XP series
	if(CMAKE_SYSTEM_VERSION MATCHES "5.0")
      set(SPECIFIC_SYSTEM_VERSION_NAME "Win2000")
	endif(CMAKE_SYSTEM_VERSION MATCHES "5.0")
	if(CMAKE_SYSTEM_VERSION MATCHES "5.1")
      set(SPECIFIC_SYSTEM_VERSION_NAME "WinXP")
	endif(CMAKE_SYSTEM_VERSION MATCHES "5.1")
	if(CMAKE_SYSTEM_VERSION MATCHES "5.2")
      set(SPECIFIC_SYSTEM_VERSION_NAME "Win2003")
	endif(CMAKE_SYSTEM_VERSION MATCHES "5.2")

	# WinVista/7 series
	if(CMAKE_SYSTEM_VERSION MATCHES "6.0")
      set(SPECIFIC_SYSTEM_VERSION_NAME "WinVISTA")
	endif(CMAKE_SYSTEM_VERSION MATCHES "6.0")
	if(CMAKE_SYSTEM_VERSION MATCHES "6.1")
	  set(SPECIFIC_SYSTEM_VERSION_NAME "Win7")
	endif(CMAKE_SYSTEM_VERSION MATCHES "6.1")

    # Compilers
	# taken from http://predef.sourceforge.net/precomp.html#sec34
	IF (MSVC)
       if(MSVC_VERSION EQUAL 1200)
	     set(SPECIFIC_COMPILER_NAME "MSVC-6.0")
       endif(MSVC_VERSION EQUAL 1200)
       if(MSVC_VERSION EQUAL 1300)
	     set(SPECIFIC_COMPILER_NAME "MSVC-7.0")
       endif(MSVC_VERSION EQUAL 1300)
       if(MSVC_VERSION EQUAL 1310)
	     set(SPECIFIC_COMPILER_NAME "MSVC-7.1-2003") #Visual Studio 2003
       endif(MSVC_VERSION EQUAL 1310)
       if(MSVC_VERSION EQUAL 1400)
	     set(SPECIFIC_COMPILER_NAME "MSVC-8.0-2005") #Visual Studio 2005
       endif(MSVC_VERSION EQUAL 1400)
       if(MSVC_VERSION EQUAL 1500)
	     set(SPECIFIC_COMPILER_NAME "MSVC-9.0-2008") #Visual Studio 2008
       endif(MSVC_VERSION EQUAL 1500)
	   if(MSVC_VERSION EQUAL 1600)
	     set(SPECIFIC_COMPILER_NAME "MSVC-10.0-2008") #Visual Studio 2008
       endif(MSVC_VERSION EQUAL 1600)
	endif(MSVC)
	IF (MINGW)
	   set(SPECIFIC_COMPILER_NAME "MinGW")
	endif(MINGW)
	IF (CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
       set(SPECIFIC_SYSTEM_VERSION_NAME "${SPECIFIC_SYSTEM_VERSION_NAME}-x86_64")
	endif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64")
endif(WIN32)

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(SPECIFIC_SYSTEM_VERSION_NAME "${CMAKE_SYSTEM_NAME}")
    if(EXISTS "/etc/issue")
      set(LINUX_NAME "")
      if(EXISTS "/etc/redhat-release")
        file(READ "/etc/redhat-release" LINUX_ISSUE)
      elseif(EXISTS "/etc/system-release")
        file(READ "/etc/system-release" LINUX_ISSUE)
      else()
        file(READ "/etc/issue" LINUX_ISSUE)
      endif()
      # Fedora case
      if(LINUX_ISSUE MATCHES "Fedora")
        string(REGEX MATCH "release ([0-9]+)" FEDORA "${LINUX_ISSUE}")
        set(LINUX_NAME "Fedora")
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "fc${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
      endif(LINUX_ISSUE MATCHES "Fedora")
      # Amazon Linux case
      # Amazon Linux release 2 (Karoo)
      if(LINUX_ISSUE MATCHES "Amazon Linux")
        string(REGEX MATCH "release ([0-9]+)" CENTOS "${LINUX_ISSUE}")
        set(LINUX_NAME "AmazonLinux")        
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "amzn${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
      endif(LINUX_ISSUE MATCHES "Amazon Linux")
      # Scientific Linux case
      # Scientific Linux SL release 5.5 (Boron)
      if(LINUX_ISSUE MATCHES "Scientific Linux")
        string(REGEX MATCH "release ([0-9]+\\.[0-9]+)" CENTOS "${LINUX_ISSUE}")
        set(LINUX_NAME "ScientificLinux")        
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "sl${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
      endif(LINUX_ISSUE MATCHES "Scientific Linux")
      # CentOS case
      # CentOS release 5.5 (Final)
      if(LINUX_ISSUE MATCHES "CentOS")
        string(REGEX MATCH "release ([0-9]+\\.[0-9]+)" CENTOS "${LINUX_ISSUE}")
        set(LINUX_NAME "CentOS")        
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "centos${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
      endif(LINUX_ISSUE MATCHES "CentOS")
      # Redhat case
      # Red Hat Enterprise Linux Server release 5 (Tikanga)
      if(LINUX_ISSUE MATCHES "Red Hat")
        string(REGEX MATCH "release ([0-9]+\\.*[0-9]*)" REDHAT "${LINUX_ISSUE}")
        set(LINUX_NAME "RedHat")        
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "rhel${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
      endif(LINUX_ISSUE MATCHES "Red Hat")
      # Ubuntu case
      if(LINUX_ISSUE MATCHES "Ubuntu")
        string(REGEX MATCH "buntu ([0-9]+\\.[0-9]+)" UBUNTU "${LINUX_ISSUE}")
        set(LINUX_NAME "Ubuntu")        
	set(LINUX_VER "${CMAKE_MATCH_1}")		  
	set(SYSTEM_NAME "Ubuntu-${CMAKE_MATCH_1}")
	set(DEB_SYSTEM_NAME "ubuntu_${CMAKE_MATCH_1}")
      endif(LINUX_ISSUE MATCHES "Ubuntu")		
      # Debian case
      if(LINUX_ISSUE MATCHES "Debian")
        string(REGEX MATCH "Debian .*ux ([0-9]+(\\.[0-9]+)?)" DEBIAN "${LINUX_ISSUE}")
        set(LINUX_NAME "Debian")
	set(LINUX_VER "${CMAKE_MATCH_1}")        
	set(SYSTEM_NAME "Debian-${CMAKE_MATCH_1}")
	set(DEB_SYSTEM_NAME "deb_${CMAKE_MATCH_1}")
      endif(LINUX_ISSUE MATCHES "Debian")
      if(LINUX_ISSUE MATCHES "Raspbian")
        string(REGEX MATCH "Raspbian .*ux ([0-9]+(\\.[0-9]+)?)" DEBIAN "${LINUX_ISSUE}")
        set(LINUX_NAME "Raspbian")
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "Debian-${CMAKE_MATCH_1}")
	set(DEB_SYSTEM_NAME "deb_${CMAKE_MATCH_1}")
      endif(LINUX_ISSUE MATCHES "Raspbian")
      # SuSE / openSUSE case
      if(LINUX_ISSUE MATCHES "openSUSE")
        string(REGEX MATCH "openSUSE ([0-9]+\\.[0-9]+)" OPENSUSE "${LINUX_ISSUE}")
        set(LINUX_NAME "openSUSE")
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "opensuse_${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
	if (LINUX_VER MATCHES "/")        
		string(REPLACE "/" "_" LINUX_VER ${LINUX_VER})        
	endif()
      elseif(LINUX_ISSUE MATCHES "SUSE")
        string(REGEX MATCH "Server ([0-9]+)" SUSE "${LINUX_ISSUE}")
        set(LINUX_NAME "sles")
	set(LINUX_VER "${CMAKE_MATCH_1}")
	set(SYSTEM_NAME "sles${CMAKE_MATCH_1}")
	set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
	if (LINUX_VER MATCHES "/")        
		string(REPLACE "/" "_" LINUX_VER ${LINUX_VER})        
	endif()
      endif()
      # Guess at the name for other Linux distros
      if(NOT SYSTEM_NAME)
	string(REGEX MATCH "([^ ]+) [^0-9]*([0-9.]+)" DISTRO "${LINUX_ISSUE}")
        set(LINUX_NAME "${CMAKE_MATCH_1}")
	set(LINUX_VER "${CMAKE_MATCH_2}")
	set(SYSTEM_NAME "${LINUX_NAME}")
	if(EXISTS "/etc/debian_version")
	  set(DEB_SYSTEM_NAME "${LINUX_NAME}")
	endif()
      endif()

	if(NOT SYSTEM_NAME)
		if(EXISTS "/etc/redhat-release")
			file(READ "/etc/redhat-release" REDHAT_RELEASE)
			string(REGEX MATCH "^Red Hat Enterprise Linux .* release ([0-9.]+)" RELEASE "${REDHAT_RELEASE}")
			set(LINUX_NAME "RedHat")
			set(LINUX_VER "${CMAKE_MATCH_1}")
			set(SYSTEM_NAME "rhel${CMAKE_MATCH_1}")
			set(RPM_SYSTEM_NAME "${SYSTEM_NAME}")
		endif(EXISTS "/etc/redhat-release")
	endif(NOT SYSTEM_NAME)

	#Find CPU Arch
	if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "64")
		set ( BIT_MODE "64")
	else()
		set ( BIT_MODE "32")
	endif ()
	
	#Find CPU Arch for Debian system 
	if ((LINUX_NAME STREQUAL "Debian") OR (LINUX_NAME STREQUAL "Ubuntu"))
		
	  # There is no such thing as i686 architecture on debian, you should use i386 instead
	  # $ dpkg --print-architecture
	  FIND_PROGRAM(DPKG_CMD dpkg)
	  IF(NOT DPKG_CMD)
		 # Cannot find dpkg in your path, default to i386
		 # Try best guess
		 if (BIT_MODE STREQUAL "32")
			SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE i386)
		 elseif (BIT_MODE STREQUAL "64")
			SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE amd64)
		 endif()
	  ENDIF(NOT DPKG_CMD)
	  EXECUTE_PROCESS(COMMAND "${DPKG_CMD}" --print-architecture
		 OUTPUT_VARIABLE CPACK_DEBIAN_PACKAGE_ARCHITECTURE
		 OUTPUT_STRIP_TRAILING_WHITESPACE
		 )
	endif ()
	
	#Find Codename for Debian system 
	if ((LINUX_NAME STREQUAL "Debian") OR (LINUX_NAME STREQUAL "Ubuntu"))
	  # $ lsb_release -cs
	  FIND_PROGRAM(LSB_CMD lsb_release)
	  IF(NOT LSB_CMD)
		 # Cannot find lsb_release in your path, default to none
		 SET(DEBIAN_CODENAME "")
	  ENDIF(NOT LSB_CMD)
	  EXECUTE_PROCESS(COMMAND "${LSB_CMD}" -cs
		 OUTPUT_VARIABLE DEBIAN_CODENAME
		 OUTPUT_STRIP_TRAILING_WHITESPACE
		 )
	endif ()

      if(LINUX_NAME) 
         set(SPECIFIC_SYSTEM_VERSION_NAME "${CMAKE_SYSTEM_NAME}-${LINUX_NAME}-${LINUX_VER}")
      else()
	set(LINUX_NAME "NOT-FOUND")
      endif(LINUX_NAME)
    endif(EXISTS "/etc/issue")

  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    # Match x.x-RELEASE, x.x-BETA1, etc.
    string(REGEX MATCH "(([0-9]+)\\.([0-9]+))-([A-Z0-9])+" FREEBSD "${CMAKE_SYSTEM_VERSION}")
    set( FREEBSD_RELEASE "${CMAKE_MATCH_1}" )
    set( FREEBSD_MAJOR "${CMAKE_MATCH_2}" )
    set( FREEBSD_MINOR "${CMAKE_MATCH_3}" )
    set( FREEBSD_VERSION "${CMAKE_SYSTEM_VERSION}" )
    set( SYSTEM_NAME "freebsd_${FREEBSD_RELEASE}" )
    set( CONDOR_FREEBSD ON )
    set( BSD_UNIX ON )
    # FIXME: Is there a >= to replace all the MATCHES operators below?
    if(FREEBSD_MAJOR MATCHES "4" )
      set( CONDOR_FREEBSD4 ON )
    elseif(FREEBSD_MAJOR MATCHES "5" )
      set( CONDOR_FREEBSD5 ON )
    elseif(FREEBSD_MAJOR MATCHES "6" ) 
      set( CONDOR_FREEBSD6 ON )
    elseif(FREEBSD_MAJOR MATCHES "7" )
      set( CONDOR_FREEBSD7 ON )
    elseif(FREEBSD_MAJOR MATCHES "8" )
      set( CONDOR_FREEBSD8 ON )
    elseif(FREEBSD_MAJOR MATCHES "9" )
      set( CONDOR_FREEBSD9 ON )
      set( CONDOR_UTMPX ON )
    elseif(FREEBSD_MAJOR MATCHES "10" )
      set( CONDOR_FREEBSD10 ON )
      set( CONDOR_UTMPX ON )
    elseif(FREEBSD_MAJOR MATCHES "11" )
      set( CONDOR_FREEBSD11 ON )
      set( CONDOR_UTMPX ON )
    elseif(FREEBSD_MAJOR MATCHES "12" )
      set( CONDOR_FREEBSD11 ON )
      set( CONDOR_UTMPX ON )
    endif()
    if( CMAKE_SYSTEM_PROCESSOR MATCHES "amd64" )
      set( SYS_ARCH "x86_64")
    elseif( CMAKE_SYSTEM_PROCESSOR MATCHES "i386" )
      set( SYS_ARCH "x86")
    endif( )
    set( PLATFORM "${SYS_ARCH}_freebsd_${FREEBSD_RELEASE}")

  elseif(OS_NAME MATCHES "DARWIN")
    set( BSD_UNIX ON )

  endif(CMAKE_SYSTEM_NAME MATCHES "Linux")

  set(SPECIFIC_SYSTEM_VERSION_NAME "${SPECIFIC_SYSTEM_VERSION_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
  set(SPECIFIC_COMPILER_NAME "")

endif(UNIX)
