 ###############################################################
 #
 # Copyright 2011 Red Hat, Inc.
 # Copyright (C) 2012-2025, Condor Team, Computer Sciences Department,
 # University of Wisconsin-Madison, WI.
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
if("${OS_NAME}" MATCHES "^WIN")
	set(WINDOWS ON)

	# The following is necessary for sdk/ddk version to compile against.
	# lowest common denominator is Vista (0x600), except when building with vc9, then we can't count on sdk support.
	add_definitions(-D_WIN32_WINNT=_WIN32_WINNT_WIN7)
	add_definitions(-DWINVER=_WIN32_WINNT_WIN7)

	# Turn on coroutine support
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:/await:strict>)

	if (MSVC90)
	    add_definitions(-DNTDDI_VERSION=NTDDI_WINXP)
	else()
	    add_definitions(-DNTDDI_VERSION=NTDDI_WIN7)
	endif()
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	set(CMD_TERM \r\n)
	set(C_WIN_BIN ${CONDOR_SOURCE_DIR}/msconfig) #${CONDOR_SOURCE_DIR}/build/backstage/win)
	#set(CMAKE_SUPPRESS_REGENERATION TRUE)

	if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
		dprint("**** IN SOURCE BUILDING ON WINDOWS IS NOT ADVISED ****")
	else()
		dprint("**** OUT OF SOURCE BUILDS ****")
		file (COPY ${CMAKE_CURRENT_SOURCE_DIR}/msconfig DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	endif()

	# when building x64 on Windows, cmake's SYS_ARCH value is wrong... so fix it here before we use it to brand the binaries.
	if ( ${CMAKE_SIZEOF_VOID_P} EQUAL 8 )
		set(SYS_ARCH "X86_64")
	endif()

endif()

# means user did not specify, so change the default.
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
	set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/release_dir" CACHE PATH "..." FORCE)
 endif()

message(STATUS "***********************************************************")
message(STATUS "System(${HOSTNAME}): ${OS_NAME}(${OS_VER}) Arch=${SYS_ARCH} BitMode=${BIT_MODE} BUILDID:${BUILDID}")
message(STATUS "install prefix:${CMAKE_INSTALL_PREFIX} cmake version: ${CMAKE_VERSION}" )
message(STATUS "********* BEGINNING CONFIGURATION *********")

##################################################
##################################################

option(WANT_PYTHON_WHEELS "Build python bindings for python wheel packaging" OFF)

# In some (near) future version, remove all references to WANT_PYTHON2_BINDINGS...
option(WANT_PYTHON2_BINDINGS "Build python bindings for python2" OFF)
option(WANT_PYTHON3_BINDINGS "Build python bindings for python3" ON)

if (WINDOWS)
	# Python 3.6 on windows will look in the PATH for dependent dll's (like boost-python)
	# Python 3.8 or later will not, so 3.6 might be preferable for some users
	option(WANT_PYTHON36 "Prefer python 3.6 to other versions of python3" OFF)
endif (WINDOWS)

# To find python in Windows we will use alternate technique
if(NOT WINDOWS)
	# Prefer "/usr/bin/python3" over "/usr/bin/python3.10"
	set(Python3_FIND_UNVERSIONED_NAMES FIRST)

	# We don't support python2 on mac (anymore)
	if (APPLE)
		set(WANT_PYTHON2_BINDINGS OFF)
	endif()

	# Wheels are build in an environment that intentionlly doesn't have the
	# python .so's or .a's installed, but does have the header files.
	# cmake find_package throws an error when you ask for development
	# artifacts, and the libraries are missing.  So, we ask for just
	# the interpreter, which find the root directory where the interpreter
	# is installed, and exports the python major/minor/patch version 
	# cmake variables.  To build the wheels, we rely on an outside script
	# to set the USE_PYTHON3_INCLUDE_DIR and USE_PYTHON3_EXT_SUFFIX

	if (WANT_PYTHON_WHEELS)
		find_package (Python3 COMPONENTS Interpreter)
		if (APPLE)
			# mac doesn't ship a python  interpeter by default
			# but we want to force the system one, not the one we found
			set(Python3_EXECUTABLE "/usr/bin/python3")
		endif()

		# All these variables are used later, and were defined in cmake 2.6
		# days.  At some point, we should not copy the find_package python
		# variables into these, and use the native cmake variables and targets.
		set(PYTHON3_EXECUTABLE        ${Python3_EXECUTABLE})
		set(PYTHON3_VERSION_STRING    ${Python3_VERSION})
		set(PYTHON3_VERSION_MAJOR     ${Python3_VERSION_MAJOR})
		set(PYTHON3_VERSION_MINOR     ${Python3_VERSION_MINOR})

		set(PYTHON3_INCLUDE_DIRS      ${USE_PYTHON3_INCLUDE_DIR})
		set(PYTHON3_MODULE_SUFFIX     ${USE_PYTHON3_EXT_SUFFIX})

		if (Python3_FOUND)
			set(PYTHON3LIBS_FOUND TRUE)
		endif()

	endif()

	if (WANT_PYTHON2_BINDINGS AND NOT WANT_PYTHON_WHEELS)

		find_package (Python2 COMPONENTS Interpreter Development)

		set(PYTHON_VERSION_STRING    ${Python2_VERSION})
		set(PYTHON_VERSION_MAJOR     ${Python2_VERSION_MAJOR})
		set(PYTHON_VERSION_MINOR     ${Python2_VERSION_MINOR})
		set(PYTHON_VERSION_PATCH     ${Python2_VERSION_PATCH})
		set(PYTHON_INCLUDE_DIRS      ${Python2_INCLUDE_DIRS})
		set(PYTHON_LIB               ${Python2_LIBRARIES})
		set(PYTHON_MODULE_EXTENSION  "${CMAKE_SHARED_LIBRARY_SUFFIX}")

		set(PYTHON_EXECUTABLE        ${Python2_EXECUTABLE})

		set(PYTHON_LIBRARIES "${PYTHON_LIB}")

		set(PYTHON_INCLUDE_PATH "${PYTHON_INCLUDE_DIRS}")
		set(PYTHONLIBS_VERSION_STRING "${PYTHON_VERSION_STRING}")
		set(PYTHON_MODULE_SUFFIX "${PYTHON_MODULE_EXTENSION}")

		if (Python2_FOUND)
			set(PYTHONLIBS_FOUND TRUE)
			message(STATUS "Python2 library found at ${PYTHON_LIB}")
		endif()

	endif(WANT_PYTHON2_BINDINGS AND NOT WANT_PYTHON_WHEELS)

	if (WANT_PYTHON3_BINDINGS AND NOT WANT_PYTHON_WHEELS)
		find_package (Python3 COMPONENTS Interpreter Development)
		if (APPLE)
			# mac doesn't ship a python  interpeter by default
			# but we want to force the system one, not the one we found
			set(Python3_EXECUTABLE "/usr/bin/python3")
		endif()

		# All these variables are used later, and were defined in cmake 2.6
		# days.  At some point, we should not copy the find_package python
		# variables into these, and use the native cmake variables and targets.
		set(PYTHON3_VERSION_STRING    ${Python3_VERSION})
		set(PYTHON3_VERSION_MAJOR     ${Python3_VERSION_MAJOR})
		set(PYTHON3_VERSION_MINOR     ${Python3_VERSION_MINOR})
		set(PYTHON3_VERSION_PATCH     ${Python3_VERSION_PATCH})
		set(PYTHON3_INCLUDE_DIRS      ${Python3_INCLUDE_DIRS})
		set(PYTHON3_LIB               ${Python3_LIBRARIES})
		#Always so, even on apple
		set(PYTHON3_MODULE_EXTENSION  ".${Python3_SOABI}.so")

		set(PYTHON3_EXECUTABLE        ${Python3_EXECUTABLE})

		set(PYTHON3_LIBRARIES "${PYTHON3_LIB}")

		set(PYTHON3_INCLUDE_PATH "${PYTHON3_INCLUDE_DIRS}")
		set(PYTHON3LIBS_VERSION_STRING "${PYTHON3_VERSION_STRING}")
		set(PYTHON3_MODULE_SUFFIX "${PYTHON3_MODULE_EXTENSION}")

		if (Python3_FOUND)
			set(PYTHON3LIBS_FOUND TRUE)
			message(STATUS "Python3 library found at ${PYTHON3_LIB}")
		endif()

	endif(WANT_PYTHON3_BINDINGS AND NOT WANT_PYTHON_WHEELS)

else(NOT WINDOWS)
    #if(WINDOWS)
    # ! less 1700 is Visual Studio 2012 or later
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
    endif(NOT (MSVC_VERSION LESS 1700))

    # starting with Visual Studio 2015, we build boost-python for several minor versions of python3
    if (NOT (MSVC_VERSION LESS 1900))
        if (WANT_PYTHON36)
            # look for a 32 bit python 3.6 before looking for the 64 python 3.6
            message(STATUS "  Looking for python 3.6 in HKLM\\Software\\Wow3264Node")
            get_filename_component(PYTHON3_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Wow6432Node\\Python\\PythonCore\\3.6\\InstallPath;]" REALPATH)
            message(STATUS "  Got ${PYTHON3_INSTALL_DIR}")
        else (WANT_PYTHON36)
            # look for python 3.9 and prefer it.  Else fallback to Python 3.10 or 3.8.
            message(STATUS "  Looking for python 3.9, 3.10, or 3.8 in that order")

			# Use the FindPython module to find the python interpreter, since
			# can find the interpreter even if it is not in the registry via the "py.exe" launcher.
			# This module was introduced in CMake 3.12
			include(FindPython)
			find_package(Python3 3.9 EXACT QUIET COMPONENTS Interpreter)
			if (NOT Python3_FOUND)
				find_package(Python3 3.10 EXACT QUIET COMPONENTS Interpreter)
			endif()
			if (NOT Python3_FOUND)
				find_package(Python3 3.8 EXACT QUIET COMPONENTS Interpreter)
			endif()

			if (Python3_FOUND)
				set(PYTHON3_INSTALL_DIR ${Python3_EXECUTABLE})
				get_filename_component(PYTHON3_INSTALL_DIR ${PYTHON3_INSTALL_DIR} DIRECTORY)
                message(STATUS "  Got ${PYTHON3_INSTALL_DIR}")
			else()
				message(STATUS "  Python 3.9, 3.10, and/or 3.8 not found")
				set(PYTHON3_INSTALL_DIR "registry")
			endif()

            # forget that we found python2
            message(STATUS "  Disabling python 2.7 build")
            set(PYTHON_INSTALL_DIR "registry")
        endif (WANT_PYTHON36)
    endif(NOT (MSVC_VERSION LESS 1900))

    # python 3.6 is our fallback if we can't find a newer version or if we are building 32 bit.
    if("${PYTHON3_INSTALL_DIR}" MATCHES "registry" OR ( CMAKE_SIZEOF_VOID_P EQUAL 8 AND "${PYTHON3_INSTALL_DIR}" MATCHES "32" ) )
        message(STATUS "  Looking for python 3.6 in HKLM\\Software")
        get_filename_component(PYTHON3_INSTALL_DIR "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Python\\PythonCore\\3.6\\InstallPath;]" REALPATH)
        message(STATUS "  Got ${PYTHON3_INSTALL_DIR}")
    endif()

    unset(PYTHON_EXECUTABLE)
    unset(PYTHON3_EXECUTABLE)
    unset(PYTHONINTERP_FOUND)

    if ("${PYTHON_INSTALL_DIR}" MATCHES "registry")
        unset(PYTHON_INSTALL_DIR)
    endif()
    if ("${PYTHON3_INSTALL_DIR}" MATCHES "registry")
        unset(PYTHON3_INSTALL_DIR)
    endif()

    # did we find either python2 or python3 ?
    if(PYTHON_INSTALL_DIR OR PYTHON3_INSTALL_DIR)
        message(STATUS "testing python for validity")
    else()
        message(STATUS "Supported python installation not found on this system")
    endif()

    if (WANT_PYTHON2_BINDINGS AND PYTHON_INSTALL_DIR)
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
                set(PYTHON_EXECUTABLE "${PYTHON_INSTALL_DIR}/python.exe")
                message(STATUS "Got PYTHON_VERSION_STRING = ${PYTHON_VERSION_STRING}")
            endif()
        endif(NOT "${PYTHON_LIBRARY_SUFFIX}" STREQUAL "27")

    endif (WANT_PYTHON2_BINDINGS AND PYTHON_INSTALL_DIR)

    if (WANT_PYTHON3_BINDINGS AND PYTHON3_INSTALL_DIR)
        set(PYTHON_QUERY_PART_01 "from distutils import sysconfig as s;")
        set(PYTHON_QUERY_PART_02 "import sys;")
        set(PYTHON_QUERY_PART_03 "import struct;")
        set(PYTHON_QUERY_PART_04 "print('.'.join(str(v) for v in sys.version_info));")
        set(PYTHON_QUERY_PART_05 "print(sys.prefix);")
        set(PYTHON_QUERY_PART_06 "print(s.get_python_inc(plat_specific=True));")
        set(PYTHON_QUERY_PART_07 "print(s.get_python_lib(plat_specific=True));")
        set(PYTHON_QUERY_PART_08 "print(s.get_config_var('EXT_SUFFIX'));")
        set(PYTHON_QUERY_PART_09 "print(hasattr(sys, 'gettotalrefcount')+0);")
        set(PYTHON_QUERY_PART_10 "print(struct.calcsize('@P'));")
        set(PYTHON_QUERY_PART_11 "print(s.get_config_var('LDVERSION') or s.get_config_var('VERSION'));")
        set(PYTHON_QUERY_PART_12 "print(sys.version_info.minor);")

        set(PYTHON3_QUERY_COMMAND "${PYTHON_QUERY_PART_01}${PYTHON_QUERY_PART_02}${PYTHON_QUERY_PART_03}${PYTHON_QUERY_PART_04}${PYTHON_QUERY_PART_05}${PYTHON_QUERY_PART_06}${PYTHON_QUERY_PART_07}${PYTHON_QUERY_PART_08}${PYTHON_QUERY_PART_09}${PYTHON_QUERY_PART_10}${PYTHON_QUERY_PART_11}${PYTHON_QUERY_PART_12}")

        execute_process(COMMAND "${PYTHON3_INSTALL_DIR}\\python.exe" "-c" "${PYTHON3_QUERY_COMMAND}"
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
        list(GET _PYTHON_VALUES 8 PYTHON_VERSION_MINOR)

        #check version (only 3.6, 3.8, 3.9, or 3.10 works for now)
        if(NOT "${PYTHON_LIBRARY_SUFFIX}" MATCHES "3[6891]")
            message(STATUS "Wrong python 3.x library version detected.  Only 3.6, 3.8, 3.9, or 3.10 supported ${PYTHON_LIBRARY_SUFFIX} detected")
        else()
            # Test for 32bit python by making sure that Python has the same pointer-size as the chosen compiler
            if(NOT "${PYTHON_SIZEOF_VOID_P}" STREQUAL "${CMAKE_SIZEOF_VOID_P}")
                message(STATUS "Python bitness mismatch: Python 3.x is ${PYTHON_SIZEOF_VOID_P} byte pointers, compiler is ${CMAKE_SIZEOF_VOID_P} byte pointers")
            else()
                message(STATUS "Valid Python 3.x version and bitdepth detected : ${PYTHON_LIBRARY_SUFFIX}")
                #we build the path to the library by hand to not be confused in multipython installations
                set(PYTHON3_LIBRARIES "${PYTHON_PREFIX}\\libs\\python${PYTHON_LIBRARY_SUFFIX}.lib")
                set(PYTHON3_LIBRARY ${PYTHON3_LIBRARIES})
                set(PYTHON3_INCLUDE_PATH "${PYTHON_INCLUDE_DIR}")
                set(PYTHON3_INCLUDE_DIRS "${PYTHON_INCLUDE_DIR}")
                message(STATUS "PYTHON3_LIBRARIES=${PYTHON3_LIBRARIES}")
                set(PYTHON3_DLL_SUFFIX "${PYTHON_MODULE_EXTENSION}")
                set(PYTHON3_MODULE_SUFFIX "${PYTHON_MODULE_EXTENSION}")
                set(PYTHON3_LIB_BASENAME "python${PYTHON_LIBRARY_SUFFIX}")
                message(STATUS "PYTHON3_DLL_SUFFIX=${PYTHON3_DLL_SUFFIX}")
                set(PYTHONLIBS_FOUND TRUE)
                set(PYTHONINTERP_FOUND TRUE)
                set(PYTHON3_VERSION_STRING "${_PYTHON_VERSION_LIST}")
                set(PYTHON3_VERSION_MINOR "${PYTHON_VERSION_MINOR}")
                set(PYTHON3_EXECUTABLE "${PYTHON3_INSTALL_DIR}/python.exe")
                message(STATUS "Got PYTHON3_VERSION_STRING = ${PYTHON3_VERSION_STRING}")
                message(STATUS "Got PYTHON3_VERSION_MINOR = ${PYTHON3_VERSION_MINOR}")
            endif()
        endif(NOT "${PYTHON_LIBRARY_SUFFIX}" MATCHES "3[6891]")

    endif (WANT_PYTHON3_BINDINGS AND PYTHON3_INSTALL_DIR)
    message(STATUS "=======================================================")

endif(NOT WINDOWS)

include (FindThreads)
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
elseif(LINUX_NAME AND NOT "${LINUX_NAME}" STREQUAL "Unknown")
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

if(CONDOR_GIT_SHA)
  add_definitions( -DCONDOR_GIT_SHA=${CONDOR_GIT_SHA} )
endif(CONDOR_GIT_SHA)

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
	  set (CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g3 -DNDEBUG")
	  set (CMAKE_C_FLAGS_RELWITHDEBINFO "-O2 -g3 -DNDEBUG")
	  set( CMAKE_BUILD_TYPE RelWithDebInfo ) # = -O2 -g (package may strip the info)
	endif()

	set( CMAKE_SUPPRESS_REGENERATION FALSE )

	find_multiple( "expat" EXPAT_FOUND )
	find_multiple( "uuid" LIBUUID_FOUND )
		# UUID appears to be available in the C runtime on Darwin.
	if (NOT LIBUUID_FOUND AND NOT APPLE )
		message(FATAL_ERROR "Could not find libuuid")
	endif()
	find_path(HAVE_UUID_UUID_H "uuid/uuid.h")
	check_include_files("sqlite3.h" HAVE_SQLITE3_H)
	find_library( SQLITE3_LIB "sqlite3" )

	check_symbol_exists(res_init "sys/types.h;netinet/in.h;arpa/nameser.h;resolv.h" HAVE_DECL_RES_INIT)
	check_symbol_exists(TCP_KEEPIDLE "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPIDLE)
	check_symbol_exists(TCP_KEEPALIVE "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPALIVE)
	check_symbol_exists(TCP_KEEPCNT "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPCNT)
	check_symbol_exists(TCP_KEEPINTVL, "sys/types.h;sys/socket.h;netinet/tcp.h" HAVE_TCP_KEEPINTVL)
	if("${OS_NAME}" STREQUAL "LINUX")
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
	check_symbol_exists(fdatasync "unistd.h" HAVE_FDATASYNC) # POSIX 2008 but MacOS as of Big Sur doesn't implement.
	check_function_exists("clock_nanosleep" HAVE_CLOCK_NANOSLEEP)

	set(HAVE_ACCESS 1) # POSIX 2001
	check_function_exists("clone" HAVE_CLONE)
	check_function_exists("euidaccess" HAVE_EUIDACCESS)
	check_function_exists("getdtablesize" HAVE_GETDTABLESIZE)
	set(HAVE_GETTIMEOFDAY 1) # POSIX 2001
	set(HAVE_MKSTEMP 1) # POSIX 2001
	check_include_files("sys/eventfd.h" HAVE_EVENTFD)
        check_function_exists("innetgr" HAVE_INNETGR)

	check_function_exists("statfs" HAVE_STATFS)
	check_function_exists("res_init" HAVE_DECL_RES_INIT)
	check_function_exists("strcasestr" HAVE_STRCASESTR)
	check_function_exists("strsignal" HAVE_STRSIGNAL)
	check_function_exists("vasprintf" HAVE_VASPRINTF)
	check_function_exists("getifaddrs" HAVE_GETIFADDRS)
	check_function_exists("readdir64" HAVE_READDIR64)

	# The backtrace library call exists, but seems to crash
	# when running under qemu ppc64le.  Let's skip that case
	if (NOT ("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "ppc64le"))
		check_function_exists("backtrace" HAVE_BACKTRACE)
	endif()

	check_function_exists("unshare" HAVE_UNSHARE)

	# we can likely put many of the checks below in here.
	check_include_files("inttypes.h" HAVE_INTTYPES_H)
	check_include_files("net/if.h" HAVE_NET_IF_H)
	check_include_files("os_types.h" HAVE_OS_TYPES_H)
	check_include_files("resolv.h" HAVE_RESOLV_H)
	check_include_files("sys/param.h" HAVE_SYS_PARAM_H)
	check_include_files("sys/types.h" HAVE_SYS_TYPES_H)
	check_include_files("procfs.h" HAVE_PROCFS_H)
	check_include_files("sys/procfs.h" HAVE_SYS_PROCFS_H)

	check_struct_has_member("struct inotify_event" "len" "sys/inotify.h" HAVE_INOTIFY)
	check_struct_has_member("struct ifconf" "ifc_len" "sys/socket.h;net/if.h" HAVE_STRUCT_IFCONF)
	check_struct_has_member("struct ifreq" "ifr_name" "sys/socket.h;net/if.h" HAVE_STRUCT_IFREQ)
	check_struct_has_member("struct ifreq" ifr_hwaddr "sys/socket.h;net/if.h" HAVE_STRUCT_IFREQ_IFR_HWADDR)

	check_struct_has_member("struct sockaddr_in" sin_len "netinet/in.h" HAVE_STRUCT_SOCKADDR_IN_SIN_LEN)

	if (NOT APPLE)
		check_struct_has_member("struct statfs" f_fstypename "sys/statfs.h" HAVE_STRUCT_STATFS_F_FSTYPENAME )
	endif()

	# the follow arg checks should be a posix check.
	# previously they were ~=check_cxx_source_compiles
	set(STATFS_ARGS "2")
	set(SIGWAIT_ARGS "2")

	check_function_exists("sched_setaffinity" HAVE_SCHED_SETAFFINITY)

	option(HAVE_HTTP_PUBLIC_FILES "Support for public input file transfer via HTTP" ON)
endif()

find_program(LN ln)
find_program(SPHINXBUILD NAMES sphinx-build sphinx-1.0-build)

# Check for the existense of and size of various types
check_type_size("id_t" ID_T)
check_type_size("__int64" __INT64)
check_type_size("int64_t" INT64_T)
check_type_size("long" LONG_INTEGER)
set(SIZEOF_LONG "${LONG_INTEGER}")
check_type_size("long long" LONG_LONG)
if(HAVE_LONG_LONG)
  set(SIZEOF_LONG_LONG "${LONG_LONG}")
endif()


##################################################
##################################################
# Now checking *nix OS based options
if("${OS_NAME}" STREQUAL "LINUX")

	set(LINUX ON)
	set( CONDOR_BUILD_SHARED_LIBS TRUE )

	check_symbol_exists(SIOCETHTOOL "linux/sockios.h" HAVE_DECL_SIOCETHTOOL)
	check_symbol_exists(SIOCGIFCONF "linux/sockios.h" HAVE_DECL_SIOCGIFCONF)
	check_include_files("linux/types.h" HAVE_LINUX_TYPES_H)
	check_include_files("linux/types.h;linux/ethtool.h" HAVE_LINUX_ETHTOOL_H)
	check_include_files("linux/sockios.h" HAVE_LINUX_SOCKIOS_H)
	check_include_files("X11/Xlib.h" HAVE_XLIB_H)
	check_include_files("X11/extensions/scrnsaver.h" HAVE_XSS_H)

	if (HAVE_XLIB_H)
	  find_library(HAVE_X11 X11)
	endif()

    if (HAVE_XSS_H)
	  find_library(HAVE_XSS Xss)
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

	set(HAVE_PTHREADS TRUE)

	# Even if the flavor of linux we are compiling on doesn't
	# have Pss in /proc/pid/smaps, the binaries we generate
	# may run on some other version of linux that does, so
	# be optimistic here.
	set(HAVE_PSS ON)

	set(HAVE_GNU_LD ON)

    option(WITH_BLAHP "Compiling the blahp" ON)

	# Does libcurl use NSS for security?
	# We need to employ some workarounds for NSS problems.
	execute_process(COMMAND /usr/bin/curl --version
		COMMAND grep -q NSS
		RESULT_VARIABLE CURL_NSS_TEST)
	if (CURL_NSS_TEST EQUAL 0)
		set(CURL_USES_NSS TRUE)
	endif()

	# Our fedora build is almost warning-clean.  Let's keep
	# it that way.
	if (EXISTS "/etc/fedora-release") 
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
	endif()

elseif(APPLE)
	add_definitions(-DDarwin)
	# CRUFT Remove this variable. All cmake files should be using APPLE.
	set(DARWIN ON)
	set( CONDOR_BUILD_SHARED_LIBS TRUE )
	check_struct_has_member("struct statfs" f_fstypename "sys/param.h;sys/mount.h" HAVE_STRUCT_STATFS_F_FSTYPENAME)
	find_library( IOKIT_FOUND IOKit )
	find_library( COREFOUNDATION_FOUND CoreFoundation )
	set(CMAKE_STRIP ${CMAKE_SOURCE_DIR}/src/condor_scripts/macosx_strip CACHE FILEPATH "Command to remove sybols from binaries" FORCE)

	set(HAVE_PTHREADS TRUE)
endif()

##################################################
##################################################
# compilation/build options.
option(HAVE_HIBERNATION "Support for condor controlled hibernation" ON)
option(HAVE_JOB_HOOKS "Enable job hook functionality" ON)
option(HAVE_BACKFILL "Compiling support for any backfill system" ON)
option(HAVE_BOINC "Compiling support for backfill with BOINC" OFF)
option(SOFT_IS_HARD "Enable strict checking for WITH_<LIB>" OFF)
option(WANT_MAN_PAGES "Generate man pages as part of the default build" OFF)
option(ENABLE_JAVA_TESTS "Enable java tests" ON)
if (WINDOWS OR APPLE)
	option(WITH_PYTHON_BINDINGS "Support for HTCondor python bindings" OFF)
else()
	option(WITH_PYTHON_BINDINGS "Support for HTCondor python bindings" ON)
endif()
option(WITH_PYTHON_BINDINGS_V2 "Support for HTCondor V2 python bindings" ON)
option(WITH_PYTHON_BINDINGS_V3 "Support for HTCondor V3 python bindings" ON)
option(BUILD_DAEMONS "Build not just libraries, but also the daemons" ON)
option(WITH_ADDRESS_SANITIZER "Build with address sanitizer" OFF)
option(WITH_UB_SANITIZER "Build with undefined behavior sanitizer" OFF)
option(DOCKER_ALLOW_RUN_AS_ROOT "Support for allow docker universe jobs to run as root inside their container" OFF)
if (LINUX)
	option(WITH_GANGLIA "Compiling with support for GANGLIA" ON)
endif(LINUX)

#####################################
# PROPER option
if (APPLE OR WINDOWS)
  option(PROPER "Try to build using native env" OFF)
else()
  option(PROPER "Try to build using native env" ON)
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

# We use @executable_path and @loader_path to find our shared libraries
# on macOS, not @rpath.
set(CMAKE_MACOSX_RPATH OFF)

if (WITH_ADDRESS_SANITIZER)
	# Condor daemons dup stderr to /dev/null, so to see output need to run with
	# ASAN_OPTIONS="log_path=/tmp/asan" condor_master 
	add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address -fno-omit-frame-pointer")
endif()

if (WITH_UB_SANITIZER)
	# Condor daemons dup stderr to /dev/null, so to see output need to run with
	# UBSAN_OPTIONS="log_path=/tmp/asan print_stacktrace=true" condor_master 
	add_compile_options(-fsanitize=undefined -fno-omit-frame-pointer)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_C_FLAGS} -fsanitize=undefined -fno-omit-frame-pointer")
endif()


#####################################
# KBDD option
if (NOT WINDOWS)
    if (HAVE_X11)
        if (NOT ("${HAVE_X11}" STREQUAL "HAVE_X11-NOTFOUND"))
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
    if ( APPLE )
        set( SFTP_SERVER "/usr/libexec/sftp-server" )
    elseif ("${LINUX_NAME}" MATCHES "openSUSE")  # suse just has to be different
        set( SFTP_SERVER "/usr/lib/ssh/sftp-server" )
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
endif()

# directory that externals are downloaded from. may be a local directory
# http or https url.
if (NOT EXTERNALS_SOURCE_URL)
   set (EXTERNALS_SOURCE_URL "https://htcss-downloads.chtc.wisc.edu/externals")
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

if (NOT EXISTS ${EXTERNAL_STAGE})
	file ( MAKE_DIRECTORY ${EXTERNAL_STAGE} )
endif()

if (NOT WINDOWS)
    # compiling everything with -fPIC is needed to dynamically load libraries
	add_compile_options(-fPIC)
endif()

#####################################
# Do we want to link in libssl and kerberos or dlopen() them at runtime?
if (NOT DEFINED DLOPEN_SECURITY_LIBS)
	if (LINUX AND NOT WANT_PYTHON_WHEELS)
		set(DLOPEN_SECURITY_LIBS TRUE)
	else()
		set(DLOPEN_SECURITY_LIBS FALSE)
	endif()
endif()

################################################################################
# Various externals rely on make, even if we're not using
# Make.  Ensure we have a usable, reasonable default for them.
if("${CMAKE_GENERATOR}" STREQUAL "Unix Makefiles")
	set( MAKE $(MAKE) )
else ()
	include (ProcessorCount)
	ProcessorCount(NUM_PROCESSORS)
	set( MAKE make -j${NUM_PROCESSORS} )
endif()

# Common externals
add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/pcre2/10.44)
add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/krb5/1.19.2)
add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/curl/8.4.0)

if (WINDOWS)
	if (WITH_PYTHON_BINDINGS)
		if (PYTHON3_VERSION_MINOR LESS 8 AND MSVC_VERSION LESS 1920)
			# boost 1.68 is vc141 and has python 2.7, 3.6, 3.8 and 3.9
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.68.0)
		else()
			# boost 1.78 is vc140 with Python 3.6, 3.8, 3.9 and 3.10
			#            or vc143 with Python 3.8 and 3.9 and 3.10
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.78.0)
		endif()
	endif()
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/1.1.1m)
else ()

	if (WITH_PYTHON_BINDINGS)
		if (APPLE)
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.68.0)
		else()
			add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/boost/1.66.0)
		endif()
	endif()

	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/openssl/packaged)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/munge/0.5.13)
	add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/scitokens-cpp/1.1.3)

	if (LINUX)
		option(WITH_LIBVIRT "Enable VM universe by linking with libvirt" ON)
		if (WITH_LIBVIRT)
			find_package(LIBVIRT REQUIRED)
		endif()
		add_subdirectory(${CONDOR_EXTERNAL_DIR}/bundles/voms/2.1.0)
	endif()

endif(WINDOWS)

add_subdirectory(${CONDOR_SOURCE_DIR}/src/safefile)

# We'll do the installation ourselves, below
set (FMT_INSTALL false)

add_subdirectory(${CONDOR_SOURCE_DIR}/src/vendor/fmt-10.1.0)

# Remove when we have C++23 everywhere
include_directories(${CONDOR_SOURCE_DIR}/src/vendor/zip-views-1.0)

# But don't try to install the header files anywhere
set_target_properties(fmt PROPERTIES PUBLIC_HEADER "")
install(TARGETS fmt
	LIBRARY DESTINATION "${C_LIB}"
	ARCHIVE DESTINATION "${C_LIB}"
	RUNTIME DESTINATION "${C_LIB}")

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

message(STATUS "********* External configuration complete (dropping config.h) *********")
dprint("CONDOR_EXTERNALS=${CONDOR_EXTERNALS}")

########################################################
configure_file(${CONDOR_SOURCE_DIR}/src/condor_includes/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.tmp)
# only update config.h if it is necessary b/c it causes massive rebuilding.
execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.tmp ${CMAKE_CURRENT_BINARY_DIR}/src/condor_includes/config.h )
add_definitions(-DHAVE_CONFIG_H)

# We could run the safefile configure script each time with cmake - or we could just fix the one usage of configure.
if (NOT WINDOWS)
    execute_process( COMMAND sed "s|#undef id_t|#cmakedefine ID_T\\n#if !defined(ID_T)\\n#define id_t uid_t\\n#endif|" ${CONDOR_SOURCE_DIR}/src/safefile/safe_id_range_list.h.in OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.in.tmp  )
    configure_file( ${CONDOR_BINARY_DIR}/src/safefile/safe_id_range_list.h.in.tmp ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.tmp_out)
	execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h.tmp_out ${CMAKE_CURRENT_BINARY_DIR}/src/safefile/safe_id_range_list.h )
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
include_directories(${CONDOR_SOURCE_DIR}/src/vendor/jwt-cpp/include)
# set these so contrib modules can add to their include path without being reliant on specific directory names.
set (CONDOR_MASTER_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_master.V6)
set (CONDOR_COLLECTOR_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_collector.V6)
set (CONDOR_NEGOTIATOR_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_negotiator.V6)
set (CONDOR_SCHEDD_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_schedd.V6)
set (CONDOR_STARTD_SRC_DIR ${CONDOR_SOURCE_DIR}/src/condor_startd.V6)

###########################################

###########################################
#extra build flags and link libs.

###########################################
# order of the below elements is important, do not touch unless you know what you are doing.
# otherwise you will break due to stub collisions.
set (SECURITY_LIBS crypto)
if (DLOPEN_SECURITY_LIBS)
	if (WITH_SCITOKENS)
		list (PREPEND SECURITY_LIBS SciTokens-headers)
	endif()
else()
	list (PREPEND SECURITY_LIBS "${VOMS_FOUND};${EXPAT_FOUND};${MUNGE_FOUND};openssl;${KRB5_FOUND}")
	if (WITH_SCITOKENS)
		list (PREPEND SECURITY_LIBS SciTokens)
	endif()
endif()

###########################################
# -lrt is required for aio_* functions.
if (LINUX)
	set(RT_FOUND "rt")
endif()


set (CONDOR_LIBS "condor_utils")
set (CONDOR_TOOL_LIBS "condor_utils")
set (CONDOR_SCRIPT_PERMS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
if (LINUX)
	set (CONDOR_LIBS_FOR_SHADOW "condor_utils_s")
else ()
  set (CONDOR_LIBS_FOR_SHADOW "${CONDOR_LIBS}")
endif ()

message(STATUS "----- Begin compiler options/flags check -----")

if (CONDOR_C_FLAGS)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${CONDOR_C_FLAGS}")
endif()

if (OPENMP_FOUND)
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:${OpenMP_CXX_FLAGS}>)
	add_compile_options($<$<COMPILE_LANGUAGE:C>:${OpenMP_C_FLAGS}>)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

if(MSVC)
	#disable autolink settings
	add_definitions(-DBOOST_ALL_NO_LIB)

	add_compile_options(/FC)      # use full paths names in errors and warnings
	if(MSVC_ANALYZE)
		# turn on code analysis.
		# also disable 6211 (leak because of exception). we use new but not catch so this warning is just noise
		add_compile_options(/analyze /wd6211) # turn on code analysis (level 6 warnings)
	endif(MSVC_ANALYZE)

	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4251")  #
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4275")  #
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4996")  # deprecation warnings
	#set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /wd4273")  # inconsistent dll linkage
	add_compile_options(/wd6334) # turn on code analysis (level 6 warnings)

	set(CONDOR_WIN_LIBS "crypt32.lib;mpr.lib;psapi.lib;mswsock.lib;netapi32.lib;imagehlp.lib;ws2_32.lib;powrprof.lib;iphlpapi.lib;userenv.lib;Pdh.lib")
else(MSVC)

	check_c_compiler_flag(-Wall c_Wall)
	if (c_Wall)
		add_compile_options(-Wall)
	endif(c_Wall)

	# This generates a million warnings, some of which represent
	# serious bugs.  At your leisure, uncomment and fix some 
	# of them.
	#check_c_compiler_flag(-Wconversion c_Wconversion)
	#if (c_Wconversion)
	#	add_compile_options(-Wconversion)
	#endif(c_Wconversion)

	check_c_compiler_flag(-W c_W)
	if (c_W)
		add_compile_options(-W)
	endif(c_W)

	check_c_compiler_flag(-Wextra c_Wextra)
	if (c_Wextra)
		add_compile_options(-Wextra)
	endif(c_Wextra)

	check_c_compiler_flag(-Wfloat-equal c_Wfloat_equal)
	if (c_Wfloat_equal)
		add_compile_options(-Wfloat-equal)
	endif(c_Wfloat_equal)

	#check_c_compiler_flag(-Wshadow c_Wshadow)
	#if (c_Wshadow)
	#	add_compile_options(-Wshadow)
	#endif(c_Wshadow)

	# someone else can enable this, as it overshadows all other warnings and can be wrong.
	# check_c_compiler_flag(-Wunreachable-code c_Wunreachable_code)
	# if (c_Wunreachable_code)
	#	add_compile_options(-Wunreachable-code)
	# endif(c_Wunreachable_code)

	check_c_compiler_flag(-Wendif-labels c_Wendif_labels)
	if (c_Wendif_labels)
		add_compile_options(-Wendif-labels)
	endif(c_Wendif_labels)

	check_c_compiler_flag(-Wpointer-arith c_Wpointer_arith)
	if (c_Wpointer_arith)
		add_compile_options(-Wpointer-arith)
	endif(c_Wpointer_arith)

	check_c_compiler_flag(-Wcast-qual c_Wcast_qual)
	if (c_Wcast_qual)
		add_compile_options(-Wcast-qual)
	endif(c_Wcast_qual)

	check_c_compiler_flag(-Wcast-align c_Wcast_align)
	if (c_Wcast_align)
		add_compile_options(-Wcast-align)
	endif(c_Wcast_align)

	check_c_compiler_flag(-Wvolatile-register-var c_Wvolatile_register_var)
	if (c_Wvolatile_register_var)
		add_compile_options(-Wvolatile-register-var)
	endif(c_Wvolatile_register_var)

	check_c_compiler_flag(-Wunused-local-typedefs c_Wunused_local_typedefs)
	if (c_Wunused_local_typedefs AND NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang" )
		# we don't ever want the 'unused local typedefs' warning treated as an error.
		add_compile_options(-Wno-error=unused-local-typedefs)
	endif(c_Wunused_local_typedefs AND NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang")

	# check compiler flag not working for this flag.
	check_c_compiler_flag(-Wdeprecated-declarations c_Wdeprecated_declarations)
	if (c_Wdeprecated_declarations)
		# we use deprecated declarations ourselves during refactoring,
		# so we always want them treated as warnings and not errors
		add_compile_options(-Wdeprecated-declarations -Wno-error=deprecated-declarations)
	endif(c_Wdeprecated_declarations)

	check_c_compiler_flag(-Wnonnull-compare c_Wnonnull_compare)
	if (c_Wnonnull_compare)
		add_compile_options(-Wno-nonnull-compare -Wno-error=nonnull-compare)
	endif(c_Wnonnull_compare)

	check_c_compiler_flag(-fstack-protector c_fstack_protector)
	if (c_fstack_protector)
		add_compile_options(-fstack-protector)
	endif(c_fstack_protector)

	# Clang on Mac OS X doesn't support -rdynamic, but the
	# check below claims it does. This is probably because the compiler
	# just prints a warning, rather than failing.
	if ( NOT "${CMAKE_C_COMPILER_ID}" MATCHES "Clang" )
		check_c_compiler_flag(-rdynamic c_rdynamic)
		if (c_rdynamic)
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")
		endif(c_rdynamic)
	endif()


	# rpmbuild/debian set _FORTIFY_SOURCE to 2 or 3 depending on platform
	# If already set, let's trust that they set it to the right value
	# otherwise set it to 2 (most portable)

	if (LINUX)
		string(FIND "${CMAKE_CXX_FLAGS}" "-D_FORTIFY_SOURCE" ALREADY_FORTIFIED)
		if (${ALREADY_FORTIFIED} EQUAL -1)
			set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=2")
		endif()
	endif()

	if (LINUX)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--warn-once -Wl,--warn-common")
		if ( "${LINUX_NAME}" STREQUAL "Ubuntu" )
			set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-as-needed")
		endif()
	endif(LINUX)

	if (LINUX)
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--enable-new-dtags")
		set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--enable-new-dtags")
	endif(LINUX)

	if (HAVE_PTHREADS AND NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
		set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pthread")
	endif(HAVE_PTHREADS AND NOT "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")

	check_cxx_compiler_flag(-shared HAVE_CC_SHARED)

	add_definitions(-D${SYS_ARCH}=${SYS_ARCH})

	check_include_file_cxx("coroutine" HAVE_NOFLAG_NEEDED_CORO)
	check_include_file_cxx("coroutine" HAVE_FLAG_NEEDED_CORO "-fcoroutines")

	if (HAVE_NOFLAG_NEEDED_CORO)
		message(STATUS "No additional flags needed for coroutines")
	else()
		if (HAVE_FLAG_NEEDED_CORO)
			add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fcoroutines>)
		else()
			message(STATUS "Cannot find proper coroutine compiler flags")
		endif()
	endif()

	# Append C flags list to C++ flags list.
	# Currently, there are no flags that are only valid for C files.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CMAKE_C_FLAGS}")

endif(MSVC)

message(STATUS "----- End compiler options/flags check -----")
message(STATUS "----- Begin CMake Var DUMP -----")
message(STATUS "SPHINXBUILD: ${SPHINXBUILD}")

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

# tell CMake to search first in directories listed in CMAKE_MODULE_PATH
# when you use FIND_PACKAGE() or INCLUDE()
dprint ( "CMAKE_MODULE_PATH: ${CMAKE_MODULE_PATH}" )

# print out where we are installing to.
dprint ( "CMAKE_INSTALL_PREFIX: ${CMAKE_INSTALL_PREFIX}" )

# this is the complete path of the cmake which runs currently (e.g. /usr/local/bin/cmake)
dprint ( "CMAKE_COMMAND: ${CMAKE_COMMAND}" )

# this is the CMake installation directory
dprint ( "CMAKE_ROOT: ${CMAKE_ROOT}" )

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

# OS support
dprint ( "UNIX: ${UNIX} Linux: ${LINUX_NAME} WIN32: ${WIN32} APPLE: ${APPLE}" )

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

# the compiler used for C++ files
dprint ( "CMAKE_CXX_COMPILER: ${CMAKE_CXX_COMPILER}" )

message(STATUS "----- End CMake Var DUMP -----")

message(STATUS "********* ENDING CONFIGURATION *********")
