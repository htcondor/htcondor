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

# OS pre mods
if(${OS_NAME} STREQUAL "DARWIN")
	# All recent versions of Mac OS X are 64-bit, but 'uname -p'
	# (the source for SYS_ARCH) reports 'i386'.
	# Override that to set the actual architecture.
	set (SYS_ARCH "X86_64")
elseif(${OS_NAME} MATCHES "WIN")
	cmake_minimum_required(VERSION 2.8.3)
	set(WINDOWS ON)

	# The following is necessary for sdk/ddk version to compile against.
	# lowest common denominator is Vista (0x600), except when building with vc9, then we can't count on sdk support.
	add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN7)
	add_definitions(-DWINVER=_WIN32_WINNT_WIN7)
	if (MSVC90)
	    add_definitions(-DNTDDI_VERSION=NTDDI_WINXP)
	else()
	    add_definitions(-DNTDDI_VERSION=NTDDI_WIN7)
	endif()
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	
	if(NOT (MSVC_VERSION LESS 1700))
		set(PREFER_CPP11 TRUE)
	endif()

	set(CMD_TERM \r\n)
	set(C_WIN_BIN ${CONDOR_SOURCE_DIR}/msconfig) #${CONDOR_SOURCE_DIR}/build/backstage/win)
	set(BISON_SIMPLE ${C_WIN_BIN}/bison.simple)
	#set(CMAKE_SUPPRESS_REGENERATION TRUE)

	set (HAVE_SNPRINTF 1)
	set (HAVE_WORKING_SNPRINTF 1)

	if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
		dprint("**** IN SOURCE BUILDING ON WINDOWS IS NOT ADVISED ****")
	else()
		dprint("**** OUT OF SOURCE BUILDS ****")
		file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/msconfig DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	endif()
    
	# means user did not specify, so change the default.
	if ( ${CMAKE_INSTALL_PREFIX} MATCHES "Program Files" )
		# mimic *nix for consistency
		set( CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/release_dir")
	endif()

	# when building x64 on Windows, cmake's SYS_ARCH value is wrong... so fix it here before we use it to brand the binaries.
	if ( ${CMAKE_SIZEOF_VOID_P} EQUAL 8 )
		set(SYS_ARCH "X86_64")
	endif()

	dprint("TODO FEATURE-> Z:TANNENBA:TJ:TSTCLAIR Update registry + paths to use this prefixed debug loc (test_install)")

endif()

  
message(STATUS "***********************************************************")
message(STATUS "System(${HOSTNAME}): ${OS_NAME}(${OS_VER}) Arch=${SYS_ARCH} BitMode=${BIT_MODE} BUILDID:${BUILDID}")
message(STATUS "install prefix:${CMAKE_INSTALL_PREFIX}")
message(STATUS "********* BEGINNING CONFIGURATION *********")

##################################################
##################################################

# To find python in Windows we will use alternate technique
option(WANT_PYTHON_WHEELS "Build python bindings for python wheel packaging" OFF)
if(NOT WINDOWS)
    if((${OS_NAME} STREQUAL "DARWIN") OR WANT_PYTHON_WHEELS)
        include (FindPythonInterp)
        message(STATUS "Got PYTHON_VERSION_STRING = ${PYTHON_VERSION_STRING}")
        # As of cmake 2.8.8, the variable below is defined by FindPythonInterp.
        # This helps ensure we get the same version of the libraries and python
        # on systems with both python2 and python3.
        if (DEFINED PYTHON_VERSION_STRING)
            set(Python_ADDITIONAL_VERSIONS "${PYTHON_VERSION_STRING}")
        endif()
        include (FindPythonLibs)
        message(STATUS "Got PYTHONLIBS_VERSION_STRING = ${PYTHONLIBS_VERSION_STRING}")
    else()
        # We need to do this the hard way for both python2 and python3 support in the same build
        # This will be easier in cmake 3
        find_program(PYTHON_EXECUTABLE python2)
        if (PYTHON_EXECUTABLE)
            set(PYTHONINTERP_FOUND TRUE)
            set(PYTHON_QUERY_PART_01 "from distutils import sysconfig;")
            set(PYTHON_QUERY_PART_02 "import sys;")
            set(PYTHON_QUERY_PART_03 "print(str(sys.version_info[0]) + '.' + str(sys.version_info[1]) + '.' + str(sys.version_info[2]));")
            set(PYTHON_QUERY_PART_04 "print(sys.version_info[0]);")
            set(PYTHON_QUERY_PART_05 "print(sys.version_info[1]);")
            set(PYTHON_QUERY_PART_06 "print(sys.version_info[2]);")
            set(PYTHON_QUERY_PART_07 "print(sysconfig.get_python_inc(plat_specific=True));")
            set(PYTHON_QUERY_PART_08 "print(sysconfig.get_config_var('LIBDIR'));")
            set(PYTHON_QUERY_PART_09 "print(sysconfig.get_config_var('MULTIARCH'));")
            set(PYTHON_QUERY_PART_10 "print(sysconfig.get_config_var('LDLIBRARY'));")
            set(PYTHON_QUERY_PART_11 "print(sysconfig.get_python_lib(1)[5:]);")

            set(PYTHON_QUERY_COMMAND "${PYTHON_QUERY_PART_01}${PYTHON_QUERY_PART_02}${PYTHON_QUERY_PART_03}${PYTHON_QUERY_PART_04}${PYTHON_QUERY_PART_05}${PYTHON_QUERY_PART_06}${PYTHON_QUERY_PART_07}${PYTHON_QUERY_PART_08}${PYTHON_QUERY_PART_09}${PYTHON_QUERY_PART_10}${PYTHON_QUERY_PART_11}")
            execute_process(COMMAND "${PYTHON_EXECUTABLE}" "-c" "${PYTHON_QUERY_COMMAND}"
                            RESULT_VARIABLE _PYTHON_SUCCESS
                            OUTPUT_VARIABLE _PYTHON_VALUES
                            ERROR_VARIABLE _PYTHON_ERROR_VALUE
                            OUTPUT_STRIP_TRAILING_WHITESPACE)

            # Convert the process output into a list
            string(REGEX REPLACE ";" "\\\\;" _PYTHON_VALUES ${_PYTHON_VALUES})
            string(REGEX REPLACE "\n" ";" _PYTHON_VALUES ${_PYTHON_VALUES})
            list(GET _PYTHON_VALUES 0 PYTHON_VERSION_STRING)
            list(GET _PYTHON_VALUES 1 PYTHON_VERSION_MAJOR)
            list(GET _PYTHON_VALUES 2 PYTHON_VERSION_MINOR)
            list(GET _PYTHON_VALUES 3 PYTHON_VERSION_PATCH)
            list(GET _PYTHON_VALUES 4 PYTHON_INCLUDE_DIRS)
            list(GET _PYTHON_VALUES 5 PYTHON_LIBDIR)
            list(GET _PYTHON_VALUES 6 PYTHON_MULTIARCH)
            list(GET _PYTHON_VALUES 7 PYTHON_LIB)
            list(GET _PYTHON_VALUES 8 C_PYTHONARCHLIB)
            set(C_PYTHONARCHLIB ${C_PYTHONARCHLIB})
            if(EXISTS "${PYTHON_LIBDIR}/${PYTHON_LIB}")
                set(PYTHON_LIBRARIES "${PYTHON_LIBDIR}/${PYTHON_LIB}")
                set(PYTHONLIBS_FOUND TRUE)
            endif()
            if(EXISTS "${PYTHON_LIBDIR}/${PYTHON_MULTIARCH}/${PYTHON_LIB}")
                set(PYTHON_LIBRARIES "${PYTHON_LIBDIR}/${PYTHON_MULTIARCH}/${PYTHON_LIB}")
                set(PYTHONLIBS_FOUND TRUE)
            endif()
            set(PYTHON_INCLUDE_PATH "${PYTHON_INCLUDE_DIRS}")
            set(PYTHONLIBS_VERSION_STRING "${PYTHON_VERSION_STRING}")

            message(STATUS "PYTHON_LIBRARIES = ${PYTHON_LIBRARIES}")
            message(STATUS "PYTHON_INCLUDE_PATH = ${PYTHON_INCLUDE_PATH}")
            message(STATUS "PYTHON_VERSION_STRING = ${PYTHON_VERSION_STRING}")
        endif()
        find_program(PYTHON3_EXECUTABLE python3)
        if (PYTHON3_EXECUTABLE)
            set(PYTHON3INTERP_FOUND TRUE)
            set(PYTHON_QUERY_PART_01 "from distutils import sysconfig;")
            set(PYTHON_QUERY_PART_02 "import sys;")
            set(PYTHON_QUERY_PART_03 "print(str(sys.version_info.major) + '.' + str(sys.version_info.minor) + '.' + str(sys.version_info.micro));")
            set(PYTHON_QUERY_PART_04 "print(sys.version_info.major);")
            set(PYTHON_QUERY_PART_05 "print(sys.version_info.minor);")
            set(PYTHON_QUERY_PART_06 "print(sys.version_info.micro);")
            set(PYTHON_QUERY_PART_07 "print(sysconfig.get_python_inc(plat_specific=True));")
            set(PYTHON_QUERY_PART_08 "print(sysconfig.get_config_var('LIBDIR'));")
            set(PYTHON_QUERY_PART_09 "print(sysconfig.get_config_var('MULTIARCH'));")
            set(PYTHON_QUERY_PART_10 "print(sysconfig.get_config_var('LDLIBRARY'));")
            set(PYTHON_QUERY_PART_11 "print(sysconfig.get_python_lib(1)[5:]);")

            set(PYTHON_QUERY_COMMAND "${PYTHON_QUERY_PART_01}${PYTHON_QUERY_PART_02}${PYTHON_QUERY_PART_03}${PYTHON_QUERY_PART_04}${PYTHON_QUERY_PART_05}${PYTHON_QUERY_PART_06}${PYTHON_QUERY_PART_07}${PYTHON_QUERY_PART_08}${PYTHON_QUERY_PART_09}${PYTHON_QUERY_PART_10}${PYTHON_QUERY_PART_11}")
            execute_process(COMMAND "${PYTHON3_EXECUTABLE}" "-c" "${PYTHON_QUERY_COMMAND}"
                            RESULT_VARIABLE _PYTHON_SUCCESS
                            OUTPUT_VARIABLE _PYTHON_VALUES
                            ERROR_VARIABLE _PYTHON_ERROR_VALUE
                            OUTPUT_STRIP_TRAILING_WHITESPACE)

            # Convert the process output into a list
            string(REGEX REPLACE ";" "\\\\;" _PYTHON_VALUES ${_PYTHON_VALUES})
            string(REGEX REPLACE "\n" ";" _PYTHON_VALUES ${_PYTHON_VALUES})
            list(GET _PYTHON_VALUES 0 PYTHON3_VERSION_STRING)
            list(GET _PYTHON_VALUES 1 PYTHON3_VERSION_MAJOR)
            list(GET _PYTHON_VALUES 2 PYTHON3_VERSION_MINOR)
            list(GET _PYTHON_VALUES 3 PYTHON3_VERSION_PATCH)
            list(GET _PYTHON_VALUES 4 PYTHON3_INCLUDE_DIRS)
            list(GET _PYTHON_VALUES 5 PYTHON3_LIBDIR)
            list(GET _PYTHON_VALUES 6 PYTHON3_MULTIARCH)
            list(GET _PYTHON_VALUES 7 PYTHON3_LIB)
            list(GET _PYTHON_VALUES 8 C_PYTHON3ARCHLIB)
            set(C_PYTHON3ARCHLIB ${C_PYTHON3ARCHLIB})
            if(EXISTS "${PYTHON3_LIBDIR}/${PYTHON3_LIB}")
                set(PYTHON3_LIBRARIES "${PYTHON3_LIBDIR}/${PYTHON3_LIB}")
                set(PYTHON3LIBS_FOUND TRUE)
            endif()
            if(EXISTS "${PYTHON3_LIBDIR}/${PYTHON3_MULTIARCH}/${PYTHON3_LIB}")
                set(PYTHON3_LIBRARIES "${PYTHON3_LIBDIR}/${PYTHON3_MULTIARCH}/${PYTHON3_LIB}")
                set(PYTHON3LIBS_FOUND TRUE)
            endif()
            set(PYTHON3_INCLUDE_PATH "${PYTHON3_INCLUDE_DIRS}")
            set(PYTHON3LIBS_VERSION_STRING "${PYTHON3_VERSION_STRING}")

            message(STATUS "PYTHON3_LIBRARIES = ${PYTHON3_LIBRARIES}")
            message(STATUS "PYTHON3_INCLUDE_PATH = ${PYTHON3_INCLUDE_PATH}")
            message(STATUS "PYTHON3_VERSION_STRING = ${PYTHON3_VERSION_STRING}")
        endif()
    endif()
    else()
        if(WINDOWS)
            #only for Visual Studio 2012
            if(NOT (MSVC_VERSION LESS 1700))
                message(STATUS "=======================================================")
                message(STATUS "Searching for python installation(s)")
                #look at registry for 32-bit view of 64-bit registry first
                message(STATUS "  Looking for python 2.7 in HKLM\\Software\\Wow3264Node")
			get_filename_component(PYTHON_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Python\\PythonCore\\2.7\\InstallPath;]" REALPATH)
			#when registry reading fails cmake returns with c:\registry
			message(STATUS "  Got ${PYTHON_INSTALL_DIR}")

			#look at native registry if not found
			if("${PYTHON_INSTALL_DIR}" MATCHES "registry" OR ( CMAKE_SIZEOF_VOID_P EQUAL 8 AND "${PYTHON_INSTALL_DIR}" MATCHES "32" ) )
				message(STATUS "  Looking for python 2.7 in HKLM\\Software")
				get_filename_component(PYTHON_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\2.7\\InstallPath;]" REALPATH)
				message(STATUS "  Got ${PYTHON_INSTALL_DIR}")
			endif()
		
			message(STATUS "  Looking for python 3.6 in HKLM\\Software\\Wow3264Node")
			get_filename_component(PYTHON3_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Python\\PythonCore\\3.6\\InstallPath;]" REALPATH)
			message(STATUS "  Got ${PYTHON3_INSTALL_DIR}")
			if("${PYTHON3_INSTALL_DIR}" MATCHES "registry" OR ( CMAKE_SIZEOF_VOID_P EQUAL 8 AND "${PYTHON3_INSTALL_DIR}" MATCHES "32" ) )
				message(STATUS "  Looking for python 3.6 in HKLM\\Software")
				get_filename_component(PYTHON3_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.6\\InstallPath;]" REALPATH)
				message(STATUS "  Got ${PYTHON3_INSTALL_DIR}")
			endif()

			unset(PYTHON_EXECUTABLE)
			unset(PYTHON3_EXECUTABLE)

			if("${PYTHON_INSTALL_DIR}" MATCHES "registry" AND "${PYTHON3_INSTALL_DIR}" MATCHES "registry")
				message(STATUS "Supported python installation not found on this system")
				unset(PYTHONINTERP_FOUND)
			else()
				message(STATUS "testing python for validity")
				set(PYTHONINTERP_FOUND TRUE)
			endif()

			if(PYTHONINTERP_FOUND)
				set(PYTHON_QUERY_PART_01 "from distutils import sysconfig as s;")
				set(PYTHON_QUERY_PART_02 "import sys;")
				set(PYTHON_QUERY_PART_03 "import struct;")
				set(PYTHON_QUERY_PART_04 "print('.'.join(str(v) for v in sys.version_info));")
				set(PYTHON_QUERY_PART_05 "print(sys.prefix);")
				set(PYTHON_QUERY_PART_06 "print(s.get_python_inc(plat_specific=True));")
				set(PYTHON_QUERY_PART_07 "print(s.get_python_lib(plat_specific=True));")
				set(PYTHON_QUERY_PART_08 "print(s.get_config_var('SO'));")
				set(PYTHON_QUERY_PART_09 "print(hasattr(sys, 'gettotalrefcount')+0);")
				set(PYTHON_QUERY_PART_10 "print(struct.calcsize('@P'));")
				set(PYTHON_QUERY_PART_11 "print(s.get_config_var('LDVERSION') or s.get_config_var('VERSION'));")
				
				set(PYTHON_QUERY_COMMAND "${PYTHON_QUERY_PART_01}${PYTHON_QUERY_PART_02}${PYTHON_QUERY_PART_03}${PYTHON_QUERY_PART_04}${PYTHON_QUERY_PART_05}${PYTHON_QUERY_PART_06}${PYTHON_QUERY_PART_07}${PYTHON_QUERY_PART_08}${PYTHON_QUERY_PART_09}${PYTHON_QUERY_PART_10}${PYTHON_QUERY_PART_11}")
				
				if( NOT "${PYTHON_INSTALL_DIR}" MATCHES "registry")
					execute_process(COMMAND "${PYTHON_INSTALL_DIR}\\python.exe" "-c" "${PYTHON_QUERY_COMMAND}" 
									RESULT_VARIABLE _PYTHON_SUCCESS
									OUTPUT_VARIABLE _PYTHON_VALUES
									ERROR_VARIABLE _PYTHON_ERROR_VALUE
									OUTPUT_STRIP_TRAILING_WHITESPACE)

					# Convert the process output into a list
					string(REGEX REPLACE ";" "\\\\;" _PYTHON_VALUES ${_PYTHON_VALUES})
					string(REGEX REPLACE "\n" ";" _PYTHON_VALUES ${_PYTHON_VALUES})
					list(GET _PYTHON_VALUES 0 _PYTHON_VERSION_LIST)
					list(GET _PYTHON_VALUES 1 PYTHON_PREFIX)
					list(GET _PYTHON_VALUES 2 PYTHON_INCLUDE_DIR)
					list(GET _PYTHON_VALUES 3 PYTHON_SITE_PACKAGES)
					list(GET _PYTHON_VALUES 4 PYTHON_MODULE_EXTENSION)
					list(GET _PYTHON_VALUES 5 PYTHON_IS_DEBUG)
					list(GET _PYTHON_VALUES 6 PYTHON_SIZEOF_VOID_P)
					list(GET _PYTHON_VALUES 7 PYTHON_LIBRARY_SUFFIX)

					#check version (only 2.7 works for now)
					if(NOT "${PYTHON_LIBRARY_SUFFIX}" STREQUAL "27")
						message(STATUS "Wrong python 2.x library version detected.  Only 2.7.x supported ${PYTHON_LIBRARY_SUFFIX} detected")
						unset(PYTHONINTERP_FOUND)
					else()
						# Test for 32bit python by making sure that Python has the same pointer-size as the chosen compiler
						if(NOT "${PYTHON_SIZEOF_VOID_P}" STREQUAL "${CMAKE_SIZEOF_VOID_P}")
							message(STATUS "Python bitness mismatch: Python 2.7 is ${PYTHON_SIZEOF_VOID_P} byte pointers, compiler is ${CMAKE_SIZEOF_VOID_P} byte pointers")
						else()
							message(STATUS "Valid Python 2.7 version and bitdepth detected")
							#we build the path to the library by hand to not be confused in multipython installations
							set(PYTHON_LIBRARIES "${PYTHON_PREFIX}\\libs\\python${PYTHON_LIBRARY_SUFFIX}.lib")
							set(PYTHON_LIBRARY ${PYTHON_LIBRARIES})
							set(PYTHON_INCLUDE_PATH "${PYTHON_INCLUDE_DIR}")
							set(PYTHON_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
							message(STATUS "PYTHON_LIBRARIES=${PYTHON_LIBRARIES}")
							set(PYTHON_DLL_SUFFIX "${PYTHON_MODULE_EXTENSION}")
							message(STATUS "PYTHON_DLL_SUFFIX=${PYTHON_DLL_SUFFIX}")
							set(PYTHONLIBS_FOUND TRUE)
							set(PYTHONINTERP_FOUND TRUE)
							set(PYTHON_VERSION_STRING "${_PYTHON_VERSION_LIST}")
							message(STATUS "Got PYTHON_VERSION_STRING = ${PYTHON_VERSION_STRING}")
						endif()
					endif()
				endif()

				if( NOT "${PYTHON3_INSTALL_DIR}" MATCHES "registry")
					execute_process(COMMAND "${PYTHON3_INSTALL_DIR}\\python.exe" "-c" "${PYTHON_QUERY_COMMAND}" 
									RESULT_VARIABLE _PYTHON_SUCCESS
									OUTPUT_VARIABLE _PYTHON_VALUES
									ERROR_VARIABLE _PYTHON_ERROR_VALUE
									OUTPUT_STRIP_TRAILING_WHITESPACE)

					# Convert the process output into a list
					string(REGEX REPLACE ";" "\\\\;" _PYTHON_VALUES ${_PYTHON_VALUES})
					string(REGEX REPLACE "\n" ";" _PYTHON_VALUES ${_PYTHON_VALUES})
					list(GET _PYTHON_VALUES 0 _PYTHON_VERSION_LIST)
					list(GET _PYTHON_VALUES 1 PYTHON_PREFIX)
					list(GET _PYTHON_VALUES 2 PYTHON_INCLUDE_DIR)
					list(GET _PYTHON_VALUES 3 PYTHON_SITE_PACKAGES)
					list(GET _PYTHON_VALUES 4 PYTHON_MODULE_EXTENSION)
					list(GET _PYTHON_VALUES 5 PYTHON_IS_DEBUG)
					list(GET _PYTHON_VALUES 6 PYTHON_SIZEOF_VOID_P)
					list(GET _PYTHON_VALUES 7 PYTHON_LIBRARY_SUFFIX)

					#check version (only 2.7 works for now)
					if(NOT "${PYTHON_LIBRARY_SUFFIX}" STREQUAL "36")
						message(STATUS "Wrong python 3.x library version detected.  Only 3.6.x supported ${PYTHON_LIBRARY_SUFFIX} detected")
						unset(PYTHONINTERP_FOUND)
					else()
						# Test for 32bit python by making sure that Python has the same pointer-size as the chosen compiler
						if(NOT "${PYTHON_SIZEOF_VOID_P}" STREQUAL "${CMAKE_SIZEOF_VOID_P}")
							message(STATUS "Python bitness mismatch: Python 3.6 is ${PYTHON_SIZEOF_VOID_P} byte pointers, compiler is ${CMAKE_SIZEOF_VOID_P} byte pointers")
						else()
							message(STATUS "Valid Python 3.6 version and bitdepth detected")
							#we build the path to the library by hand to not be confused in multipython installations
							set(PYTHON3_LIBRARIES "${PYTHON_PREFIX}\\libs\\python${PYTHON_LIBRARY_SUFFIX}.lib")
							set(PYTHON3_LIBRARY ${PYTHON3_LIBRARIES})
							set(PYTHON3_INCLUDE_PATH "${PYTHON_INCLUDE_DIR}")
							set(PYTHON3_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
							message(STATUS "PYTHON3_LIBRARIES=${PYTHON3_LIBRARIES}")
							set(PYTHON3_DLL_SUFFIX "${PYTHON_MODULE_EXTENSION}")
							message(STATUS "PYTHON3_DLL_SUFFIX=${PYTHON3_DLL_SUFFIX}")
							set(PYTHONLIBS_FOUND TRUE)
							set(PYTHONINTERP_FOUND TRUE)
							set(PYTHON3_VERSION_STRING "${_PYTHON_VERSION_LIST}")
							message(STATUS "Got PYTHON3_VERSION_STRING = ${PYTHON3_VERSION_STRING}")
						endif()
					endif()
				endif()

			endif()
			message(STATUS "=======================================================")
		endif()
	endif(WINDOWS)
endif()
include (FindThreads)
include (GlibcDetect)
if (WINDOWS)
	message(STATUS "OpenMP support will be disabled on Windows until we have a chance to fix the installer to support it")
	# TJ: 8.5.8 disabling OpenMP on Windows because it adds a dependency on an additional merge module for VCOMP110.DLL
else()
	find_package ("OpenMP")
endif()

if (FIPS_BUILD)
    add_definitions(-DFIPS_MODE=1)
endif()

add_definitions(-D${OS_NAME}="${OS_NAME}_${OS_VER}")
if (CONDOR_PLATFORM)
    add_definitions(-DPLATFORM="${CONDOR_PLATFORM}")
elseif(PLATFORM)
    add_definitions(-DPLATFORM="${PLATFORM}")
elseif(LINUX_NAME AND NOT ${LINUX_NAME} STREQUAL "Unknown")
    add_definitions(-DPLATFORM="${SYS_ARCH}-${LINUX_NAME}_${LINUX_VER}")
else()
    add_definitions(-DPLATFORM="${SYS_ARCH}-${OS_NAME}_${OS_VER}")
endif()

if(PRE_RELEASE)
  add_definitions( -DPRE_RELEASE_STR=" ${PRE_RELEASE}" )
else()
  add_definitions( -DPRE_RELEASE_STR="" )
endif(PRE_RELEASE)
add_definitions( -DCONDOR_VERSION="${VERSION}" )

if(PACKAGEID)
  add_definitions( -DPACKAGEID=${PACKAGEID} )
endif(PACKAGEID)

if( NOT BUILD_DATE )
  GET_DATE( BUILD_DATE )
endif()
if(BUILD_DATE)
  add_definitions( -DBUILD_DATE="${BUILD_DATE}" )
endif()

set( CONDOR_EXTERNAL_DIR ${CONDOR_SOURCE_DIR}/externals )

# set to true to enable printing of make actions
set( CMAKE_VERBOSE_MAKEFILE FALSE )

# set to true if we should build and use shared libraries where appropriate
set( CONDOR_BUILD_SHARED_LIBS FALSE )

# Windows is so different perform the check 1st and start setting the vars.
if( NOT WINDOWS)

	set( CMD_TERM && )

	if (_DEBUG)
	  set( CMAKE_BUILD_TYPE Debug )
	else()
	  # Using -O2 crashes the compiler on ppc mac os x when compiling
	  # condor_submit
	  if ( ${OS_NAME} STREQUAL "DARWIN" AND ${SYS_ARCH} STREQUAL "POWERPC" )
	    set( CMAKE_BUILD_TYPE Debug ) # = -g (package may strip the info)
	  else()

            add_definitions(-D_FORTIFY_SOURCE=2)
	    set( CMAKE_BUILD_TYPE RelWithDebInfo ) # = -O2 -g (package may strip the info)
	  endif()
	endif()

	set( CMAKE_SUPPRESS_REGENERATION FALSE )

	set(HAVE_PTHREAD_H ${CMAKE_HAVE_PTHREAD_H})

	find_path(HAVE_OPENSSL_SSL_H "openssl/ssl.h")

	if ( ${OS_NAME} STREQUAL "DARWIN" )
		# Mac OS X includes the pcre library but not the header
		# file. Supply a copy ourselves.
		include_directories( ${CONDOR_EXTERNAL_DIR}/bundles/pcre/7.6/include-darwin )
		set( HAVE_PCRE_H "${CONDOR_EXTERNAL_DIR}/bundles/pcre/7.6/include-darwin" CACHE PATH "Path to pcre header." )
	else()
		if( SYSTEM_NAME STREQUAL "sl6.3" )
			include_directories( ${CONDOR_EXTERNAL_DIR}/bundles/pcre/8.40/include )
			set( HAVE_PCRE_H FALSE )
			set( HAVE_PCRE_PCRE_H FALSE )
		else()
			find_path(HAVE_PCRE_H "pcre.h")
			find_path(HAVE_PCRE_PCRE_H "pcre/pcre.h" )
		endif()
	endif()

    find_multiple( "z" ZLIB_FOUND)
	find_multiple( "expat" EXPAT_FOUND )
	find_multiple( "uuid" LIBUUID_FOUND )
	find_path(HAVE_UUID_UUID_H "uuid/uuid.h")
	find_library( HAVE_DMTCP dmtcpaware HINTS /usr/local/lib/dmtcp )
	find_multiple( "resolv" HAVE_LIBRESOLV )
    find_multiple ("dl" HAVE_LIBDL )
	find_library( HAVE_LIBLTDL "ltdl" )
	find_multiple( "cares" HAVE_LIBCARES )
	# On RedHat6, there's a libcares19 package, but no libcares
	find_multiple( "cares19" HAVE_LIBCARES19 )
	find_multiple( "pam" HAVE_LIBPAM )
	find_program( BISON bison )
	find_program( FLEX flex )
	find_program( AUTOCONF autoconf )
	find_program( AUTOMAKE automake )
	find_program( LIBTOOLIZE libtoolize )
	check_include_files("sqlite3.h" HAVE_SQLITE3_H)
	find_library( SQLITE3_LIB "sqlite3" )

	check_library_exists(dl dlopen "" HAVE_DLOPEN)
	check_symbol_exists(res_init "sys/types.h;netinet/in.h;arpa/nameser.h;resolv.h" HAVE_DECL_RES_INIT)
	check_symbol_exists(TCP_KEEPIDLE "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPIDLE)
	check_symbol_exists(TCP_KEEPALIVE "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPALIVE)
	check_symbol_exists(TCP_KEEPCNT "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPCNT)
	check_symbol_exists(TCP_KEEPINTVL, "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPINTVL)
	if(${OS_NAME} STREQUAL "LINUX")
		check_include_files("linux/tcp.h" HAVE_LINUX_TCP_H)
	endif()
	if( HAVE_LINUX_TCP_H )
		check_symbol_exists(TCP_USER_TIMEOUT, "linux/tcp.h;sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_USER_TIMEOUT)
	else()
		check_symbol_exists(TCP_USER_TIMEOUT, "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_USER_TIMEOUT)
	endif()
	check_symbol_exists(MS_PRIVATE "sys/mount.h" HAVE_MS_PRIVATE)
	check_symbol_exists(MS_SHARED  "sys/mount.h" HAVE_MS_SHARED)
	check_symbol_exists(MS_SLAVE  "sys/mount.h" HAVE_MS_SLAVE)
	check_symbol_exists(MS_REC  "sys/mount.h" HAVE_MS_REC)
	# Python also defines HAVE_EPOLL; hence, we use non-standard 'CONDOR_HAVE_EPOLL' here.
	check_symbol_exists(epoll_create1 "sys/epoll.h" CONDOR_HAVE_EPOLL)
	check_symbol_exists(poll "sys/poll.h" CONDOR_HAVE_POLL)
	check_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC)
	check_function_exists("clock_gettime" HAVE_CLOCK_GETTIME)
	check_symbol_exists(CLOCK_MONOTONIC_RAW "time.h" HAVE_CLOCK_MONOTONIC_RAW)
	check_symbol_exists(CLOCK_REALTIME_COARSE "time.h" HAVE_CLOCK_REALTIME_COARSE)
	check_function_exists("clock_nanosleep" HAVE_CLOCK_NANOSLEEP)

	check_function_exists("access" HAVE_ACCESS)
	check_function_exists("clone" HAVE_CLONE)
	check_function_exists("dirfd" HAVE_DIRFD)
	check_function_exists("euidaccess" HAVE_EUIDACCESS)
	check_function_exists("execl" HAVE_EXECL)
	check_function_exists("fstat64" HAVE_FSTAT64)
	check_function_exists("_fstati64" HAVE__FSTATI64)
	check_function_exists("getdtablesize" HAVE_GETDTABLESIZE)
	check_function_exists("getpagesize" HAVE_GETPAGESIZE)
	check_function_exists("gettimeofday" HAVE_GETTIMEOFDAY)
	check_function_exists("inet_ntoa" HAS_INET_NTOA)
	check_function_exists("lchown" HAVE_LCHOWN)
	check_function_exists("lstat" HAVE_LSTAT)
	check_function_exists("lstat64" HAVE_LSTAT64)
	check_function_exists("_lstati64" HAVE__LSTATI64)
	check_function_exists("mkstemp" HAVE_MKSTEMP)
	check_function_exists("setegid" HAVE_SETEGID)
	check_function_exists("setenv" HAVE_SETENV)
	check_function_exists("seteuid" HAVE_SETEUID)
	check_function_exists("setlinebuf" HAVE_SETLINEBUF)
	check_function_exists("snprintf" HAVE_SNPRINTF)
	check_function_exists("snprintf" HAVE_WORKING_SNPRINTF)
	check_include_files("sys/eventfd.h" HAVE_EVENTFD)
        check_function_exists("innetgr" HAVE_INNETGR)
        check_function_exists("getgrnam" HAVE_GETGRNAM)

	check_function_exists("stat64" HAVE_STAT64)
	check_function_exists("_stati64" HAVE__STATI64)
	check_function_exists("statfs" HAVE_STATFS)
	check_function_exists("statvfs" HAVE_STATVFS)
	check_function_exists("res_init" HAVE_DECL_RES_INIT)
	check_function_exists("strcasestr" HAVE_STRCASESTR)
	check_function_exists("strsignal" HAVE_STRSIGNAL)
	check_function_exists("unsetenv" HAVE_UNSETENV)
	check_function_exists("vasprintf" HAVE_VASPRINTF)
	check_function_exists("getifaddrs" HAVE_GETIFADDRS)
	check_function_exists("readdir64" HAVE_READDIR64)
	check_function_exists("backtrace" HAVE_BACKTRACE)
	check_function_exists("unshare" HAVE_UNSHARE)
	check_function_exists("proc_pid_rusage" HAVE_PROC_PID_RUSAGE)

	# we can likely put many of the checks below in here.
	check_include_files("dlfcn.h" HAVE_DLFCN_H)
	check_include_files("inttypes.h" HAVE_INTTYPES_H)
	check_include_files("ldap.h" HAVE_LDAP_H)
	find_multiple( "ldap;lber" LDAP_FOUND )
	check_include_files("net/if.h" HAVE_NET_IF_H)
	check_include_files("os_types.h" HAVE_OS_TYPES_H)
	check_include_files("resolv.h" HAVE_RESOLV_H)
	check_include_files("sys/mount.h" HAVE_SYS_MOUNT_H)
	check_include_files("sys/param.h" HAVE_SYS_PARAM_H)
	check_include_files("sys/personality.h" HAVE_SYS_PERSONALITY_H)
	check_include_files("sys/syscall.h" HAVE_SYS_SYSCALL_H)
	check_include_files("sys/statfs.h" HAVE_SYS_STATFS_H)
	check_include_files("sys/statvfs.h" HAVE_SYS_STATVFS_H)
	check_include_files("sys/types.h" HAVE_SYS_TYPES_H)
	check_include_files("sys/vfs.h" HAVE_SYS_VFS_H)
	check_include_files("stdint.h" HAVE_STDINT_H)
	check_include_files("ustat.h" HAVE_USTAT_H)
	check_include_files("valgrind.h" HAVE_VALGRIND_H)
	check_include_files("procfs.h" HAVE_PROCFS_H)
	check_include_files("sys/procfs.h" HAVE_SYS_PROCFS_H)

	check_type_exists("struct inotify_event" "sys/inotify.h" HAVE_INOTIFY)
	check_type_exists("struct ifconf" "sys/socket.h;net/if.h" HAVE_STRUCT_IFCONF)
	check_type_exists("struct ifreq" "sys/socket.h;net/if.h" HAVE_STRUCT_IFREQ)
	check_struct_has_member("struct ifreq" ifr_hwaddr "sys/socket.h;net/if.h" HAVE_STRUCT_IFREQ_IFR_HWADDR)

	check_struct_has_member("struct sockaddr_in" sin_len "netinet/in.h" HAVE_STRUCT_SOCKADDR_IN_SIN_LEN)

	check_struct_has_member("struct statfs" f_fstyp "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYP)
	if (NOT ${OS_NAME} STREQUAL "DARWIN")
  	  if( HAVE_SYS_STATFS_H )
		check_struct_has_member("struct statfs" f_fstypename "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYPENAME )
	  else()
		check_struct_has_member("struct statfs" f_fstypename "sys/mount.h" HAVE_STRUCT_STATFS_F_FSTYPENAME )
	  endif()
	endif()
	check_struct_has_member("struct statfs" f_type "sys/statfs.h" HAVE_STRUCT_STATFS_F_TYPE)
	check_struct_has_member("struct statvfs" f_basetype "sys/types.h;sys/statvfs.h" HAVE_STRUCT_STATVFS_F_BASETYPE)

	# the follow arg checks should be a posix check.
	# previously they were ~=check_cxx_source_compiles
	set(STATFS_ARGS "2")
	set(SIGWAIT_ARGS "2")

	check_cxx_source_compiles("
		#include <sched.h>
		int main() {
			cpu_set_t s;
			sched_setaffinity(0, 1024, &s);
			return 0;
		}
		" HAVE_SCHED_SETAFFINITY )

	check_cxx_source_compiles("
		#include <sched.h>
		int main() {
			cpu_set_t s;
			sched_setaffinity(0, &s);
			return 0;
		}
		" HAVE_SCHED_SETAFFINITY_2ARG )

	if(HAVE_SCHED_SETAFFINITY_2ARG)
		set(HAVE_SCHED_SETAFFINITY ON)
	endif()

	check_cxx_compiler_flag(-std=c++11 cxx_11)
	check_cxx_compiler_flag(-std=c++0x cxx_0x)
	if (cxx_11)

		# Some versions of Clang require an additional C++11 flag, as the default stdlib
		# is from an old GCC version.
		if ( "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" )
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
		endif()

		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

		check_cxx_source_compiles("
		#include <unordered_map>
		#include <memory>
		int main() {
			std::unordered_map<int, int> ci;
			std::shared_ptr<int> foo;
			return 0;
		}
		" PREFER_CPP11 )
	elseif(cxx_0x)
		# older g++s support some of c++11 with the c++0x flag
		# which we should try to enable, if they do not have
		# the c++11 flag.  This at least gets us std::unique_ptr

		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++0x")
	endif()

	if (NOT PREFER_CPP11)

	  # Some early 4.0 g++'s have unordered maps, but their iterators don't work
	  check_cxx_source_compiles("
		#include <tr1/unordered_map>
		int main() {
			std::tr1::unordered_map<int, int>::const_iterator ci;
			return 0;
		}
		" PREFER_TR1 )

	endif(NOT PREFER_CPP11)
	
	# note the following is fairly gcc specific, but *we* only check gcc version in std:u which it requires.
	exec_program (${CMAKE_CXX_COMPILER}
    		ARGS ${CMAKE_CXX_COMPILER_ARG1} -dumpversion
    		OUTPUT_VARIABLE CMAKE_CXX_COMPILER_VERSION )

	exec_program (${CMAKE_C_COMPILER}
    		ARGS ${CMAKE_C_COMPILER_ARG1} -dumpversion
    		OUTPUT_VARIABLE CMAKE_C_COMPILER_VERSION )

endif()

find_program(HAVE_VMWARE vmware)
find_program(LN ln)
find_program(LATEX2HTML latex2html)
find_program(LATEX latex)

# Check for the existense of and size of various types
check_type_size("id_t" ID_T)
check_type_size("__int64" __INT64)
check_type_size("int64_t" INT64_T)
check_type_size("int" INTEGER)
set(SIZEOF_INT "${INTEGER}")
check_type_size("long" LONG_INTEGER)
set(SIZEOF_LONG "${LONG_INTEGER}")
check_type_size("long long" LONG_LONG)
if(HAVE_LONG_LONG)
  set(SIZEOF_LONG_LONG "${LONG_LONG}")
endif()
check_type_size("void *" VOIDPTR)
set(SIZEOF_VOIDPTR "${VOIDPTR}")


##################################################
##################################################
# Now checking *nix OS based options
set(HAS_FLOCK ON)
set(DOES_SAVE_SIGSTATE OFF)

if (${OS_NAME} STREQUAL "SUNOS")
	set(SOLARIS ON)
	set(NEEDS_64BIT_SYSCALLS ON)
	set(NEEDS_64BIT_STRUCTS ON)
	set(DOES_SAVE_SIGSTATE ON)
	set(HAS_FLOCK OFF)
	add_definitions(-DSolaris)
	if (${OS_VER} STREQUAL "5.9")
		add_definitions(-DSolaris29)
	elseif(${OS_VER} STREQUAL "5.10")
		add_definitions(-DSolaris10)
	elseif(${OS_VER} STREQUAL "5.11")
		add_definitions(-DSolaris11)
	else()
		message(FATAL_ERROR "unknown version of Solaris")
	endif()
	add_definitions(-D_STRUCTURED_PROC)
	set(HAS_INET_NTOA ON)
	include_directories( "/usr/include/kerberosv5" )
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lkstat -lelf -lnsl -lsocket")

	#update for solaris builds to use pre-reqs namely binutils in this case
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -B$ENV{PATH}")

elseif(${OS_NAME} STREQUAL "LINUX")

	set(LINUX ON)
	set( CONDOR_BUILD_SHARED_LIBS TRUE )

	find_so_name(LIBLTDL_SO ${HAVE_LIBLTDL})

	set(DOES_SAVE_SIGSTATE ON)
	check_symbol_exists(SIOCETHTOOL "linux/sockios.h" HAVE_DECL_SIOCETHTOOL)
	check_symbol_exists(SIOCGIFCONF "linux/sockios.h" HAVE_DECL_SIOCGIFCONF)
	check_include_files("linux/types.h" HAVE_LINUX_TYPES_H)
	check_include_files("linux/types.h;linux/ethtool.h" HAVE_LINUX_ETHTOOL_H)
	check_include_files("linux/magic.h" HAVE_LINUX_MAGIC_H)
	check_include_files("linux/nfsd/const.h" HAVE_LINUX_NFSD_CONST_H)
	check_include_files("linux/personality.h" HAVE_LINUX_PERSONALITY_H)
	check_include_files("linux/sockios.h" HAVE_LINUX_SOCKIOS_H)
	check_include_files("X11/Xlib.h" HAVE_XLIB_H)
	check_include_files("X11/extensions/scrnsaver.h" HAVE_XSS_H)

	if (HAVE_XLIB_H)
	  find_library(HAVE_X11 X11)
	endif()

    if (HAVE_XSS_H)
	  find_library(HAVE_XSS Xss)
	  find_library(HAVE_XEXT Xext)
	endif()

    check_include_files("systemd/sd-daemon.h" HAVE_SD_DAEMON_H)
    if (HAVE_SD_DAEMON_H)
		# Since systemd-209, libsystemd-daemon.so has been deprecated
		# and the symbols in that library now are in libsystemd.so.
		# Since RHEL7 ships with systemd-219, it seems find for us to
		# always use libsystemd and not worry about libsystemd-daemon.
        find_library(LIBSYSTEMD_DAEMON_PATH systemd)
        find_so_name(LIBSYSTEMD_DAEMON_SO ${LIBSYSTEMD_DAEMON_PATH})
    endif()

	dprint("Threaded functionality only enabled in Linux, Windows, and Mac OS X > 10.6")
	set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})

	# Even if the flavor of linux we are compiling on doesn't
	# have Pss in /proc/pid/smaps, the binaries we generate
	# may run on some other version of linux that does, so
	# be optimistic here.
	set(HAVE_PSS ON)

	#The following checks are for std:u only.
	glibc_detect( GLIBC_VERSION )

	set(HAVE_GNU_LD ON)
    option(HAVE_HTTP_PUBLIC_FILES "Support for public input file transfer via HTTP" ON)

elseif(${OS_NAME} STREQUAL "AIX")
	set(AIX ON)
	set(DOES_SAVE_SIGSTATE ON)
	set(NEEDS_64BIT_STRUCTS ON)
elseif(${OS_NAME} STREQUAL "DARWIN")
	add_definitions(-DDarwin)
	set(DARWIN ON)
	set( CONDOR_BUILD_SHARED_LIBS TRUE )
	check_struct_has_member("struct statfs" f_fstypename "sys/param.h;sys/mount.h" HAVE_STRUCT_STATFS_F_FSTYPENAME)
	find_library( IOKIT_FOUND IOKit )
	find_library( COREFOUNDATION_FOUND CoreFoundation )
	set(CMAKE_STRIP ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_strip CACHE FILEPATH "Command to remove sybols from binaries" FORCE)

	dprint("Threaded functionality only enabled in Linux, Windows and Mac OS X > 10.6")

	check_symbol_exists(PTHREAD_RECURSIVE_MUTEX_INITIALIZER "pthread.h" HAVE_DECL_PTHREAD_RECURSIVE_MUTEX_INITIALIZER)
	check_symbol_exists(PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP "pthread.h" HAVE_DECL_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
	if (HAVE_DECL_PTHREAD_RECURSIVE_MUTEX_INITIALIZER OR HAVE_DECL_PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP)
		set(HAS_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
		set(HAVE_PTHREADS ${CMAKE_USE_PTHREADS_INIT})
	else()
		set(HAS_PTHREADS FALSE)
		set(HAVE_PTHREADS FALSE)
	endif()

	exec_program (sw_vers ARGS -productVersion OUTPUT_VARIABLE TEST_VER)
	if(${TEST_VER} MATCHES "10.([67])")
		set (HAVE_OLD_SCANDIR 1)
		dprint("Using old function signature for scandir()")
	else()
		dprint("Using POSIX function signature for scandir()")
	endif()
endif()

##################################################
##################################################
# compilation/build options.
option(UW_BUILD "Variable to allow UW-esk builds." OFF)
option(HAVE_HIBERNATION "Support for condor controlled hibernation" ON)
option(HAVE_JOB_HOOKS "Enable job hook functionality" ON)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" ON)
option(SOFT_IS_HARD "Enable strict checking for WITH_<LIB>" OFF)
option(WANT_CONTRIB "Enable building of contrib modules" OFF)
option(WANT_FULL_DEPLOYMENT "Install condors deployment scripts, libs, and includes" ON)
option(WANT_GLEXEC "Build and install condor glexec functionality" ON)
option(WANT_MAN_PAGES "Generate man pages as part of the default build" OFF)
option(ENABLE_JAVA_TESTS "Enable java tests" ON)
option(WITH_PYTHON_BINDINGS "Support for HTCondor python bindings" ON)

#####################################
# PROPER option
if (UW_BUILD OR WINDOWS)
  option(PROPER "Try to build using native env" OFF)

  # so the clipped detection will try to match glibc vers and if it fails will disable
  if (LINUX)
	option(CLIPPED "enable/disable the standard universe" ON)
  else()
	option(CLIPPED "enable/disable the standard universe" ON)
  endif()

  dprint("**TO UW: IF YOU WANT CUSTOM SETTINGS ADD THEM HERE**")

else()
  option(PROPER "Try to build using native env" ON)
  option(CLIPPED "disable the standard universe" ON)
endif()

if (NOT CLIPPED AND NOT LINUX)
	message (FATAL_ERROR "standard universe is *only* supported on Linux")
endif()

#####################################
# RPATH option
if (LINUX AND NOT PROPER)
	option(CMAKE_SKIP_RPATH "Skip RPATH on executables" OFF)
else()
	option(CMAKE_SKIP_RPATH "Skip RPATH on executables" ON)
endif()

if ( NOT CMAKE_SKIP_RPATH )
	set( CMAKE_INSTALL_RPATH ${CONDOR_RPATH} )
	set( CMAKE_BUILD_WITH_INSTALL_RPATH TRUE )
endif()

#####################################
# KBDD option
if (NOT WINDOWS)
    if (HAVE_X11)
        if (NOT (${HAVE_X11} STREQUAL "HAVE_X11-NOTFOUND"))
            option(HAVE_KBDD "Support for condor_kbdd" ON)
        endif()
    endif()
else()
    option(HAVE_KBDD "Support for condor_kbdd" ON)
endif()

#####################################
# Shared port option
option(HAVE_SHARED_PORT "Support for condor_shared_port" ON)
if (NOT WINDOWS)
	set (HAVE_SCM_RIGHTS_PASSFD ON)
endif()


#####################################
# ssh_to_job option
if (NOT WINDOWS) 
    option(HAVE_SSH_TO_JOB "Support for condor_ssh_to_job" ON)
endif()
if ( HAVE_SSH_TO_JOB )
    if ( DARWIN )
        set( SFTP_SERVER "/usr/libexec/sftp-server" )
    elseif ( DEB_SYSTEM_NAME )
        set( SFTP_SERVER "/usr/lib/openssh/sftp-server" )
    else()
	set( SFTP_SERVER "/usr/libexec/openssh/sftp-server" )
    endif()
endif()

if (BUILD_TESTING)
	set(TEST_TARGET_DIR ${CMAKE_BINARY_DIR}/src/condor_tests)
endif(BUILD_TESTING)

##################################################
##################################################
# setup for the externals, the variables defined here
# are used in the construction of externals within
# the condor build.  The point of main interest is
# how "cacheing" is performed.

if (NOT PROPER)
	message(STATUS "********* Building with UW externals *********")
	cmake_minimum_required(VERSION 2.8)
endif()

# directory that externals are downloaded from. may be a local directory
# http or https url.
if (NOT EXTERNALS_SOURCE_URL)
   set (EXTERNALS_SOURCE_URL "http://parrot.cs.wisc.edu/externals")
endif()

option(CACHED_EXTERNALS "enable/disable cached externals" OFF)
set (EXTERNAL_STAGE $ENV{CONDOR_BLD_EXTERNAL_STAGE})
if (NOT EXTERNAL_STAGE)
	if (CACHED_EXTERNALS AND NOT WINDOWS)
		set( EXTERNAL_STAGE "/scratch/condor_externals")
	else()
		set (EXTERNAL_STAGE ${CMAKE_CURRENT_BINARY_DIR}/bld_external)
	endif()
endif()

if (WINDOWS)
	string (REPLACE "\\" "/" EXTERNAL_STAGE "${EXTERNAL_STAGE}")
endif()

dprint("EXTERNAL_STAGE=${EXTERNAL_STAGE}")
if (NOT EXISTS ${EXTERNAL_STAGE})
	file ( MAKE_DIRECTORY ${EXTERNAL_STAGE} )
endif()

# I'd like this to apply to classads build as well, so I put it
# above the addition of the .../src/classads subdir:
if (LINUX
    AND PROPER 
    AND (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    AND NOT (${CMAKE_CXX_COMPILER_VERSION} VERSION_LESS 4.4.6))

    # I wrote a nice macro for testing linker flags, but it is useless
    # because at least some older versions of linker ignore all '-z'
    # args in the name of "solaris compatibility"
    # So instead I'm enabling for GNU toolchains on RHEL-6 and newer

    # note, I'm only turning these on for proper builds because
    # non-proper external builds don't receive the necessary flags
    # and it breaks the build

    # partial relro (for all libs)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-z,relro")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS}  -Wl,-z,relro")

    # full relro and pie get turned on for daemons:
    set(cxx_full_relro_and_pie 1)
    # I've seen a reference to '-z bind_now', but all the
    # versions I can find actually use just '-z now':
    set(cxx_full_relro_arg "-Wl,-z,now")
endif()

if (NOT WINDOWS)
    # compiling everything with -fPIC is needed to dynamically load libraries
    # linked against libstdc++
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
endif()

#####################################
# Do we want to link in libssl and kerberos or dlopen() them at runtime?
if (LINUX AND NOT PROPER AND NOT WANT_PYTHON_WHEELS)
	set( DLOPEN_SECURITY_LIBS TRUE )
endif()

################################################################################
# Various externals rely on make, even if we're not using
# Make.  Ensure we have a usable, reasonable default for them.
if(${CMAKE_GENERATOR} STREQUAL "Unix Makefiles")
	set( MAKE $(MAKE) )
else ()
	include (ProcessorCount)
	ProcessorCount(NUM_PROCESSORS)
	set( MAKE make -j${NUM_PROCESSORS} )
endif()

if (WINDOWS)

  if(NOT (MSVC_VERSION LESS 1900))
    set(BOOST_DOWNLOAD_WIN boost-1.68.0-VC15.tar.gz)
    #if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
    #  set(BOOST_DOWNLOAD_WIN boost-1.68.0-VC15-Win64.tar.gz)
    #else()
    #  set(BOOST_DOWNLOAD_WIN boost-1.68.0-VC15-Win32.tar.gz)
    #endif()
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.68.0)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/1.0.2l)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre/8.40)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/krb5/1.14.5)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/curl/7.54.1)
  elseif(NOT (MSVC_VERSION LESS 1700))
	if (MSVC11)
      if (CMAKE_SIZEOF_VOID_P EQUAL 8 )
        set(BOOST_DOWNLOAD_WIN boost-1.54.0-VC11-Win64_V4.tar.gz)
      else()
        set(BOOST_DOWNLOAD_WIN boost-1.54.0-VC11-Win32_V4.tar.gz)
	  endif()
	endif()
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.54.0)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/1.0.1j)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre/8.33)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/krb5/1.12)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/curl/7.33.0)
  else()
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.49.0)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/0.9.8h-p2)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre/7.6)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/krb5/1.4.3-p1)
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/curl/7.31.0-p1)
  endif()
  
  # DRMAA currently punted on Windows until we can figure out correct build
  #add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/drmaa/1.6.2)
  add_subdirectory(${CONDOR_SOURCE_DIR}/src/classad)
else ()

  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/drmaa/1.6.2)
  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/qpid/0.8-RC3)
  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.66.0)

  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/curl/7.31.0-p1 )
  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/1.0.1e)
  if( SYSTEM_NAME STREQUAL "sl6.3" )
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre/8.40)
  else()
    add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre/7.6)
  endif()
  add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/krb5/1.12)
  add_subdirectory(${CONDOR_SOURCE_DIR}/src/classad)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/unicoregahp/1.2.0)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/libxml2/2.7.3)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/libvirt/0.6.2)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/libcgroup/0.41)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/munge/0.5.13)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/scitokens-cpp/0.3.4)

	# globus is an odd *beast* which requires a bit more config.
	# old globus builds on manylinux1 (centos5 docker image)
	if (LINUX)
		if (${SYSTEM_NAME} MATCHES "centos5.11")
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/globus/5.2.5)
		else()
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/globus/6.0)
		endif()
	elseif(DARWIN)
		exec_program (sw_vers ARGS -productVersion OUTPUT_VARIABLE TEST_VER)
		if (${TEST_VER} MATCHES "10.1[3-9]")
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/globus/6.0)
		else()
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/globus/5.2.5)
		endif()
	else()
		add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/globus/5.2.5)
	endif()
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/blahp/1.16.5.1)
	# voms held back for MacOS (config issues) (2.1.0 needed for OpenSSL 1.1)
	# old voms also builds on manylinux1 (centos5 docker image)
    if (LINUX AND NOT ${SYSTEM_NAME} MATCHES "centos5.11")
        add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/voms/2.1.0)
    else()
        add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/voms/2.0.13)
    endif()
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/cream/1.15.4)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boinc/7.14.1)

        if (LINUX)
          option(WITH_GANGLIA "Compiling with support for GANGLIA" ON)
        endif(LINUX)

	# the following logic if for standard universe *only*
	if (LINUX AND NOT CLIPPED AND GLIBC_VERSION)

		add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/zlib/1.2.3)
		add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/glibc)

		if (EXT_GLIBC_FOUND)
		  if (${BIT_MODE} STREQUAL "32")
			  set (DOES_COMPRESS_CKPT ON) # this is total crap
		  endif(${BIT_MODE} STREQUAL "32")

		  if (DOES_SAVE_SIGSTATE)
			  set(STD_U_C_FLAGS -DSAVE_SIGSTATE)
		  endif(DOES_SAVE_SIGSTATE)

		  set (STD_UNIVERSE ON)

		  # seriously I've sold my soul doing this dirty work
		  set (CONDOR_COMPILE ${CONDOR_SOURCE_DIR}/src/condor_scripts/condor_compile)
		  set (CONDOR_ARCH_LINK ${CONDOR_SOURCE_DIR}/src/condor_scripts/condor_arch_link)
		  set (STDU_LIB_LOC ${CMAKE_INSTALL_PREFIX}/${C_LIB})

		  include_directories( ${CONDOR_SOURCE_DIR}/src/condor_io.std )

		  message( STATUS "** Standard Universe Enabled **")

		else()
			message( STATUS "** Standard Universe Disabled **")
		endif()
	else()
		message( STATUS "** Standard Universe Disabled **")
	endif()

endif(WINDOWS)

add_subdirectory(${CONDOR_SOURCE_DIR}/src/safefile)

if (DARWIN)
	include_directories( ${DARWIN_OPENSSL_INCLUDE} )
endif()

### addition of a single externals target which allows you to
if (CONDOR_EXTERNALS)
if (NOT WINDOWS)
	add_custom_target( externals DEPENDS ${EXTERNAL_MOD_DEP} )
	add_dependencies( externals ${CONDOR_EXTERNALS} )
else (NOT WINDOWS)
	add_custom_target( ALL_EXTERN DEPENDS ${EXTERNAL_MOD_DEP} )
	add_dependencies( ALL_EXTERN ${CONDOR_EXTERNALS} )
endif (NOT WINDOWS)	
endif(CONDOR_EXTERNALS)

######### special case for contrib
if (WANT_CONTRIB AND WITH_MANAGEMENT)
    # global scoping external linkage var when options enable.
    if (WINDOWS)
        set (CONDOR_QMF condor_qmflib;${QPID_FOUND})
    endif()
    add_definitions( -DWANT_CONTRIB )
    add_definitions( -DWITH_MANAGEMENT )
endif()

#####################################
# Do we want to link in the GSI libraries or dlopen() them at runtime?
if (HAVE_EXT_GLOBUS AND LINUX AND NOT PROPER AND NOT WANT_PYTHON_WHEELS)
	set( DLOPEN_GSI_LIBS TRUE )
endif()

message(STATUS "********* External configuration complete (dropping config.h) *********")
dprint("CONDOR_EXTERNALS=${CONDOR_EXTERNALS}")

########################################################
configure_file(${CONDOR_SOURCE_DIR}/src/condor_includes/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.tmp)
# only update config.h if it is necessary b/c it causes massive rebuilding.
exec_program ( ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.tmp ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.h )
add_definitions(-DHAVE_CONFIG_H)

# We could run the safefile configure script each time with cmake - or we could just fix the one usage of configure.
if (NOT WINDOWS)
    execute_process( COMMAND sed "s|#undef id_t|#cmakedefine ID_T\\n#if !defined(ID_T)\\n#define id_t uid_t\\n#endif|" ${CONDOR_SOURCE_DIR}/src/safefile/safe_id_range_list.h.in OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.in.tmp  )
    configure_file( ${CONDOR_BINARY_DIR}/src/safefile/safe_id_range_list.h.in.tmp ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.tmp_out)
    exec_program ( ${CMAKE_COMMAND} ARGS -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.tmp_out ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h )
endif()

###########################################
# include and link locations
include_directories( ${CONDOR_EXTERNAL_INCLUDE_DIRS} )
link_directories( ${CONDOR_EXTERNAL_LINK_DIRS} )

if ( $ENV{JAVA_HOME} )
	include_directories($ENV{JAVA_HOME}/include)
endif()

include_directories(${CONDOR_SOURCE_DIR}/src/condor_includes)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes)
include_directories(${CONDOR_SOURCE_DIR}/src/condor_utils)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/condor_utils)
set (DAEMON_CORE ${CONDOR_SOURCE_DIR}/src/condor_daemon_core.V6) #referenced elsewhere primarily for soap gen stuff
include_directories(${DAEMON_CORE})
include_directories(${CONDOR_SOURCE_DIR}/src/condor_daemon_client)
include_directories(${CONDOR_SOURCE_DIR}/src/ccb)
include_directories(${CONDOR_SOURCE_DIR}/src/condor_io)
include_directories(${CONDOR_SOURCE_DIR}/src/h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/h)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/classad)
include_directories(${CONDOR_SOURCE_DIR}/src/classad)
include_directories(${CONDOR_SOURCE_DIR}/src/safefile)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/src/safefile)
if (WANT_CONTRIB)
    include_directories(${CONDOR_SOURCE_DIR}/src/condor_contrib)
endif(WANT_CONTRIB)
# set these so contrib modules can add to their include path without being reliant on specific directory names.
set (CONDOR_MASTER_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_master.V6)
set (CONDOR_COLLECTOR_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_collector.V6)
set (CONDOR_NEGOTIATOR_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_negotiator.V6)
set (CONDOR_SCHEDD_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_schedd.V6)
set (CONDOR_STARTD_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_startd.V6)

###########################################

###########################################
#extra build flags and link libs.

if (NOT HAVE_EXT_OPENSSL)
	message (FATAL_ERROR "openssl libraries not found!")
endif()

###########################################
# order of the below elements is important, do not touch unless you know what you are doing.
# otherwise you will break due to stub collisions.
if (DLOPEN_GSI_LIBS)
	set (SECURITY_LIBS "")
	set (SECURITY_LIBS_STATIC "")
else()
	set (SECURITY_LIBS "${VOMS_FOUND};${GLOBUS_FOUND};${SCITOKENS_FOUND};${EXPAT_FOUND}")
	set (SECURITY_LIBS_STATIC "${VOMS_FOUND_STATIC};${GLOBUS_FOUND_STATIC};${EXPAT_FOUND}")
endif()

###########################################
# in order to use clock_gettime, you need to link the the rt library
# (only for linux with glibc < 2.17)
if (HAVE_CLOCK_GETTIME AND LINUX)
    set(RT_FOUND "rt")
else()
    set(RT_FOUND "")
endif()

set (CONDOR_LIBS_STATIC "condor_utils_s;classads;${SECURITY_LIBS_STATIC};${RT_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${IOKIT_FOUND};${COREFOUNDATION_FOUND};${RT_FOUND};${MUNGE_FOUND}")
set (CONDOR_LIBS "condor_utils;${RT_FOUND};${CLASSADS_FOUND};${SECURITY_LIBS};${PCRE_FOUND};${MUNGE_FOUND}")
set (CONDOR_TOOL_LIBS "condor_utils;${RT_FOUND};${CLASSADS_FOUND};${SECURITY_LIBS};${PCRE_FOUND};${MUNGE_FOUND}")
set (CONDOR_SCRIPT_PERMS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if (LINUX AND NOT PROPER)
  set (CONDOR_LIBS_FOR_SHADOW "condor_utils_s;classads;${SECURITY_LIBS};${RT_FOUND};${PCRE_FOUND};${OPENSSL_FOUND};${KRB5_FOUND};${IOKIT_FOUND};${COREFOUNDATION_FOUND};${MUNGE_FOUND}")
else ()
  set (CONDOR_LIBS_FOR_SHADOW "${CONDOR_LIBS}")
endif ()

message(STATUS "----- Begin compiler options/flags check -----")

if (CONDOR_C_FLAGS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONDOR_C_FLAGS}")
endif()
if (CONDOR_CXX_FLAGS)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CONDOR_CXX_FLAGS}")
endif()

if (OPENMP_FOUND)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

if(MSVC)
	#disable autolink settings 
	add_definitions(-DBOOST_ALL_NO_LIB)

	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /FC")      # use full paths names in errors and warnings
	if(MSVC_ANALYZE)
		# turn on code analysis. 
		# also disable 6211 (leak because of exception). we use new but not catch so this warning is just noise
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /analyze /wd6211") # turn on code analysis (level 6 warnings)
	endif(MSVC_ANALYZE)

	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4251")  #
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4275")  #
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")  # deprecation warnings
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4273")  # inconsistent dll linkage
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd6334") # inclusion warning from boost. 

	set(CONDOR_WIN_LIBS "crypt32.lib;mpr.lib;psapi.lib;mswsock.lib;netapi32.lib;imagehlp.lib;ws2_32.lib;powrprof.lib;iphlpapi.lib;userenv.lib;Pdh.lib")
else(MSVC)

	if (GLIBC_VERSION)
		add_definitions(-DGLIBC=GLIBC)
		add_definitions(-DGLIBC${GLIBC_VERSION}=GLIBC${GLIBC_VERSION})
		set(GLIBC${GLIBC_VERSION} ON)
	endif(GLIBC_VERSION)

	check_c_compiler_flag(-Wall c_Wall)
	if (c_Wall)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
	endif(c_Wall)

	# Added to help make resulting libcondor_utils smaller.
	#check_c_compiler_flag(-fno-exceptions no_exceptions)
	#if (no_exceptions)
	#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fno-exceptions")
	#	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fno-exceptions")
	#endif(no_exceptions)
	#check_c_compiler_flag(-Os c_Os)
	#if (c_Os)
	#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os")
	#	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Os")
	#endif(c_Os)

	dprint("TSTCLAIR - DISABLING -flto b/c of gcc failure in koji try again later")
	#if (CMAKE_C_COMPILER_VERSION STRGREATER "4.7.0" OR CMAKE_C_COMPILER_VERSION STREQUAL "4.7.0")
	#   
	#  check_c_compiler_flag(-flto c_lto)
	#  if (c_lto)
	#	  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -flto")
	#	  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -flto")
	#  endif(c_lto)
	#else()
	#  dprint("skipping c_lto flag check")
	#endif()

	check_c_compiler_flag(-W c_W)
	if (c_W)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -W")
	endif(c_W)

	check_c_compiler_flag(-Wextra c_Wextra)
	if (c_Wextra)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wextra")
	endif(c_Wextra)

	check_c_compiler_flag(-Wfloat-equal c_Wfloat_equal)
	if (c_Wfloat_equal)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfloat-equal")
	endif(c_Wfloat_equal)

	#check_c_compiler_flag(-Wshadow c_Wshadow)
	#if (c_Wshadow)
	#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
	#endif(c_Wshadow)

	# someone else can enable this, as it overshadows all other warnings and can be wrong.
	# check_c_compiler_flag(-Wunreachable-code c_Wunreachable_code)
	# if (c_Wunreachable_code)
	#	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunreachable-code")
	# endif(c_Wunreachable_code)

	check_c_compiler_flag(-Wendif-labels c_Wendif_labels)
	if (c_Wendif_labels)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wendif-labels")
	endif(c_Wendif_labels)

	check_c_compiler_flag(-Wpointer-arith c_Wpointer_arith)
	if (c_Wpointer_arith)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wpointer-arith")
	endif(c_Wpointer_arith)

	check_c_compiler_flag(-Wcast-qual c_Wcast_qual)
	if (c_Wcast_qual)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-qual")
	endif(c_Wcast_qual)

	check_c_compiler_flag(-Wcast-align c_Wcast_align)
	if (c_Wcast_align)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcast-align")
	endif(c_Wcast_align)

	check_c_compiler_flag(-Wvolatile-register-var c_Wvolatile_register_var)
	if (c_Wvolatile_register_var)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wvolatile-register-var")
	endif(c_Wvolatile_register_var)

	check_c_compiler_flag(-Wunused-local-typedefs c_Wunused_local_typedefs)
	if (c_Wunused_local_typedefs AND NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
		# we don't ever want the 'unused local typedefs' warning treated as an error.
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=unused-local-typedefs")
	endif(c_Wunused_local_typedefs AND NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang")

	# check compiler flag not working for this flag.  
	if (NOT CMAKE_C_COMPILER_VERSION VERSION_LESS "4.8")
	check_c_compiler_flag(-Wdeprecated-declarations c_Wdeprecated_declarations)
	if (c_Wdeprecated_declarations)
		# we use deprecated declarations ourselves during refactoring,
		# so we always want them treated as warnings and not errors
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wdeprecated-declarations -Wno-error=deprecated-declarations")
	endif(c_Wdeprecated_declarations)
	endif()

	check_c_compiler_flag(-Wnonnull-compare c_Wnonnull_compare)
	if (c_Wnonnull_compare)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-nonnull-compare -Wno-error=nonnull-compare")
	endif(c_Wnonnull_compare)

	# gcc on our AIX machines recognizes -fstack-protector, but lacks
	# the requisite library.
	if (NOT AIX)
		check_c_compiler_flag(-fstack-protector c_fstack_protector)
		if (c_fstack_protector)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector")
		endif(c_fstack_protector)
	endif(NOT AIX)

	# Clang on Mac OS X doesn't support -rdynamic, but the
	# check below claims it does. This is probably because the compiler
	# just prints a warning, rather than failing.
	if ( NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "Clang" )
		check_c_compiler_flag(-rdynamic c_rdynamic)
		if (c_rdynamic)
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
		endif(c_rdynamic)
	endif()

	if (LINUX)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--warn-once -Wl,--warn-common")
		if ( "${LINUX_NAME}" STREQUAL "Ubuntu" )
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-as-needed")
		endif()
		# Link RedHat 5 binaries with both hash styles (GNU and SYSV)
		# so that binaries are usable on old distros such as SUSE Linux Enterprise Server 10
		if ( ${SYSTEM_NAME} MATCHES "rhel5" )
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--hash-style=both")
		endif()
	endif(LINUX)

	if( HAVE_LIBDL AND NOT BSD_UNIX )
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -ldl")
	endif()

	if (AIX)
		# specifically ask for the C++ libraries to be statically linked
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-berok -Wl,-bstatic -lstdc++ -Wl,-bdynamic -lcfg -lodm -static-libgcc")
	endif(AIX)

	if ( NOT PROPER AND HAVE_LIBRESOLV )
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lresolv")
	endif()

	if (HAVE_PTHREADS AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
	endif(HAVE_PTHREADS AND NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")

	check_cxx_compiler_flag(-shared HAVE_CC_SHARED)

	if ( NOT CLIPPED AND ${SYS_ARCH} MATCHES "86")

		if (NOT ${SYS_ARCH} MATCHES "64" )
			add_definitions( -DI386=${SYS_ARCH} )
		endif()

		# set for maximum binary compatibility based on current machine arch.
		check_c_compiler_flag(-mtune=generic c_mtune)
		if (c_mtune)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mtune=generic")
		endif(c_mtune)

	endif()

	add_definitions(-D${SYS_ARCH}=${SYS_ARCH})

	# Append C flags list to C++ flags list.
	# Currently, there are no flags that are only valid for C files.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

endif(MSVC)

message(STATUS "----- End compiler options/flags check -----")
message(STATUS "----- Begin CMake Var DUMP -----")
message(STATUS "CMAKE_STRIP: ${CMAKE_STRIP}")
message(STATUS "LN: ${LN}")
message(STATUS "LATEX: ${LATEX}")
message(STATUS "LATEX2HTML: ${LATEX2HTML}")

# if you are building in-source, this is the same as CMAKE_SOURCE_DIR, otherwise
# this is the top level directory of your build tree
dprint ( "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}" )

# if you are building in-source, this is the same as CMAKE_CURRENT_SOURCE_DIR, otherwise this
# is the directory where the compiled or generated files from the current CMakeLists.txt will go to
dprint ( "CMAKE_CURRENT_BINARY_DIR: ${CMAKE_CURRENT_BINARY_DIR}" )

# this is the directory, from which cmake was started, i.e. the top level source directory
dprint ( "CMAKE_SOURCE_DIR: ${CMAKE_SOURCE_DIR}" )

# this is the directory where the currently processed CMakeLists.txt is located in
dprint ( "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}" )

# contains the full path to the top level directory of your build tree
dprint ( "PROJECT_BINARY_DIR: ${PROJECT_BINARY_DIR}" )

# contains the full path to the root of your project source directory,
# i.e. to the nearest directory where CMakeLists.txt contains the PROJECT() command
dprint ( "PROJECT_SOURCE_DIR: ${PROJECT_SOURCE_DIR}" )

# set this variable to specify a common place where CMake should put all executable files
# (instead of CMAKE_CURRENT_BINARY_DIR)
dprint ( "EXECUTABLE_OUTPUT_PATH: ${EXECUTABLE_OUTPUT_PATH}" )

# set this variable to specify a common place where CMake should put all libraries
# (instead of CMAKE_CURRENT_BINARY_DIR)
dprint ( "LIBRARY_OUTPUT_PATH: ${LIBRARY_OUTPUT_PATH}" )

# tell CMake to search first in directories listed in CMAKE_MODULE_PATH
# when you use FIND_PACKAGE() or INCLUDE()
dprint ( "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}" )

# print out where we are installing to.
dprint ( "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}" )

# this is the complete path of the cmake which runs currently (e.g. /usr/local/bin/cmake)
dprint ( "CMAKE_COMMAND: ${CMAKE_COMMAND}" )

# this is the CMake installation directory
dprint ( "CMAKE_ROOT: ${CMAKE_ROOT}" )

# this is the filename including the complete path of the file where this variable is used.
dprint ( "CMAKE_CURRENT_LIST_FILE: ${CMAKE_CURRENT_LIST_FILE}" )

# this is linenumber where the variable is used
dprint ( "CMAKE_CURRENT_LIST_LINE: ${CMAKE_CURRENT_LIST_LINE}" )

# this is used when searching for include files e.g. using the FIND_PATH() command.
dprint ( "CMAKE_INCLUDE_PATH: ${CMAKE_INCLUDE_PATH}" )

# this is used when searching for libraries e.g. using the FIND_LIBRARY() command.
dprint ( "CMAKE_LIBRARY_PATH: ${CMAKE_LIBRARY_PATH}" )

# the complete system name, e.g. "Linux-2.4.22", "FreeBSD-5.4-RELEASE" or "Windows 5.1"
dprint ( "CMAKE_SYSTEM: ${CMAKE_SYSTEM}" )

# the short system name, e.g. "Linux", "FreeBSD" or "Windows"
dprint ( "CMAKE_SYSTEM_NAME: ${CMAKE_SYSTEM_NAME}" )

# only the version part of CMAKE_SYSTEM
dprint ( "CMAKE_SYSTEM_VERSION: ${CMAKE_SYSTEM_VERSION}" )

# the processor name (e.g. "Intel(R) Pentium(R) M processor 2.00GHz")
dprint ( "CMAKE_SYSTEM_PROCESSOR: ${CMAKE_SYSTEM_PROCESSOR}" )

# the Condor src directory
dprint ( "CONDOR_SOURCE_DIR: ${CONDOR_SOURCE_DIR}" )
dprint ( "CONDOR_EXTERNAL_DIR: ${CONDOR_EXTERNAL_DIR}" )
dprint ( "TEST_TARGET_DIR: ${TEST_TARGET_DIR}" )

# the Condor version string being used
dprint ( "CONDOR_VERSION: ${CONDOR_VERSION}" )

# the build id
dprint ( "BUILDID: ${BUILDID}" )

# the build date & time
dprint ( "BUILD_TIMEDATE: ${BUILD_TIMEDATE}" )
dprint ( "BUILD_DATE: ${BUILD_DATE}" )

# the pre-release string
dprint ( "PRE_RELEASE: ${PRE_RELEASE}" )

# the platform specified
dprint ( "PLATFORM: ${PLATFORM}" )

# the Condor platform specified
dprint ( "CONDOR_PLATFORM: ${CONDOR_PLATFORM}" )

# the system name (used for generated tarballs)
dprint ( "SYSTEM_NAME: ${SYSTEM_NAME}" )

# the RPM system name (used for generated tarballs)
dprint ( "RPM_SYSTEM_NAME: ${RPM_SYSTEM_NAME}" )

# the Condor package name
dprint ( "CONDOR_PACKAGE_NAME: ${CONDOR_PACKAGE_NAME}" )

# is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
dprint ( "UNIX: ${UNIX}" )

# is TRUE on all BSD-derived UNIXen
dprint ( "BSD_UNIX: ${BSD_UNIX}" )

# is TRUE on all UNIX-like OS's, including Apple OS X and CygWin
dprint ( "Linux: ${LINUX_NAME}" )

# Print FreeBSD info
dprint ( "FreeBSD: ${FREEBSD_MAJOR}.${FREEBSD_MINOR}" )

# is TRUE on Windows, including CygWin
dprint ( "WIN32: ${WIN32}" )

# is TRUE on Apple OS X
dprint ( "APPLE: ${APPLE}" )

# is TRUE when using the MinGW compiler in Windows
dprint ( "MINGW: ${MINGW}" )

# is TRUE on Windows when using the CygWin version of cmake
dprint ( "CYGWIN: ${CYGWIN}" )

# is TRUE on Windows when using a Borland compiler
dprint ( "BORLAND: ${BORLAND}" )

if (WINDOWS)
	dprint ( "MSVC: ${MSVC}" )
	dprint ( "MSVC_VERSION: ${MSVC_VERSION}" )
	dprint ( "MSVC_IDE: ${MSVC_IDE}" )
endif(WINDOWS)

# set this to true if you don't want to rebuild the object files if the rules have changed,
# but not the actual source files or headers (e.g. if you changed the some compiler switches)
dprint ( "CMAKE_SKIP_RULE_DEPENDENCY: ${CMAKE_SKIP_RULE_DEPENDENCY}" )

# since CMake 2.1 the install rule depends on all, i.e. everything will be built before installing.
# If you don't like this, set this one to true.
dprint ( "CMAKE_SKIP_INSTALL_ALL_DEPENDENCY: ${CMAKE_SKIP_INSTALL_ALL_DEPENDENCY}" )

# If set, runtime paths are not added when using shared libraries. Default it is set to OFF
dprint ( "CMAKE_SKIP_RPATH: ${CMAKE_SKIP_RPATH}" )
dprint ( "CMAKE_INSTALL_RPATH: ${CMAKE_INSTALL_RPATH}")
dprint ( "CMAKE_BUILD_WITH_INSTALL_RPATH: ${CMAKE_BUILD_WITH_INSTALL_RPATH}")

# set this to true if you are using makefiles and want to see the full compile and link
# commands instead of only the shortened ones
dprint ( "CMAKE_VERBOSE_MAKEFILE: ${CMAKE_VERBOSE_MAKEFILE}" )

# this will cause CMake to not put in the rules that re-run CMake. This might be useful if
# you want to use the generated build files on another machine.
dprint ( "CMAKE_SUPPRESS_REGENERATION: ${CMAKE_SUPPRESS_REGENERATION}" )

# A simple way to get switches to the compiler is to use ADD_DEFINITIONS().
# But there are also two variables exactly for this purpose:

# output what the linker flags are
dprint ( "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}" )

# the compiler flags for compiling C sources
dprint ( "CMAKE_C_FLAGS: ${CMAKE_C_FLAGS}" )

# the compiler flags for compiling C++ sources
dprint ( "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}" )

# Choose the type of build.  Example: SET(CMAKE_BUILD_TYPE Debug)
dprint ( "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}" )

# if this is set to ON, then all libraries are built as shared libraries by default.
dprint ( "BUILD_SHARED_LIBS: ${BUILD_SHARED_LIBS}" )

# the compiler used for C files
dprint ( "CMAKE_C_COMPILER: ${CMAKE_C_COMPILER}" )

# version information about the compiler
dprint ( "CMAKE_C_COMPILER_VERSION: ${CMAKE_C_COMPILER_VERSION}" )

# the compiler used for C++ files
dprint ( "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}" )

# version information about the compiler
dprint ( "CMAKE_CXX_COMPILER_VERSION: ${CMAKE_CXX_COMPILER_VERSION}" )

# if the compiler is a variant of gcc, this should be set to 1
dprint ( "CMAKE_COMPILER_IS_GNUCC: ${CMAKE_COMPILER_IS_GNUCC}" )

# if the compiler is a variant of g++, this should be set to 1
dprint ( "CMAKE_COMPILER_IS_GNUCXX : ${CMAKE_COMPILER_IS_GNUCXX}" )

# the tools for creating libraries
dprint ( "CMAKE_AR: ${CMAKE_AR}" )
dprint ( "CMAKE_RANLIB: ${CMAKE_RANLIB}" )

message(STATUS "----- Begin CMake Var DUMP -----")

message(STATUS "********* ENDING CONFIGURATION *********")
