@echo off & setlocal
REM ======================================================================
REM 
REM  Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
REM  University of Wisconsin-Madison, WI.
REM  
REM  Licensed under the Apache License, Version 2.0 (the "License"); you
REM  may not use this file except in compliance with the License.  You may
REM  obtain a copy of the License at
REM  
REM     http://www.apache.org/licenses/LICENSE-2.0
REM  
REM  Unless required by applicable law or agreed to in writing, software
REM  distributed under the License is distributed on an "AS IS" BASIS,
REM  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM  See the License for the specific language governing permissions and
REM  limitations under the License.
REM 
REM ======================================================================

REM ======================================================================
REM ======================================================================
REM Main entry point
REM ======================================================================
REM ======================================================================

REM ======================================================================
REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_SRC_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR
REM ======================================================================

REM ======================================================================
REM Manages the building of the gsoap external on Win32R 
REM ======================================================================

REM Make certain TEMP environment variable exists and has
REM a trailing backslash.  We need to do this before calling
REM gmake below, since gmake will invoke bison, and bison
REM requires TEMP w/ trailing backslash.
REM Try to create a c:\temp if no TEMP env var defined
if not defined TEMP ( if not exist c:\TEMP (mkdir c:\TEMP || ( echo No TEMP env var and failed to make c:\TEMP && goto :FAIL) ) )
if not defined TEMP (set TEMP=c:\TEMP)
REM Be sure to have a trailing backslash
set TEMP=%TEMP%\

REM First, build soapcpp2.exe
copy config.WINDOWS.h %PACKAGE_SRC_NAME%\gsoap\src\config.h
copy Makefile.win32 %PACKAGE_SRC_NAME%\gsoap\src\
gmake -f Makefile.win32 -C %PACKAGE_SRC_NAME%\gsoap\src
gmake -f Makefile.win32 install -C %PACKAGE_SRC_NAME%\gsoap\src
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM Now patch the stdsoap.h and .cpp file
cd %PACKAGE_INSTALL_DIR%\src
call :PATCH %PACKAGE_BUILD_DIR%\stdsoap2.h-2.7.10-patch
call :PATCH %PACKAGE_BUILD_DIR%\stdsoap2.cpp-2.7.10-patch

REM Clean up the environment.
endlocal
goto :EOF

REM ======================================================================
REM ======================================================================
REM Functions
REM ======================================================================
REM ======================================================================

REM ======================================================================
:PATCH
REM ======================================================================
REM Call patch on the for a file in the current directory
REM ======================================================================
patch --verbose --binary -i %1
if %ERRORLEVEL% neq 0 goto :FAIL
exit 0

REM ======================================================================
:FAIL
REM ======================================================================
REM All the failure calls
REM ======================================================================
echo. & echo *** Building %PACKAGE_SRC_NAME% failed (ERROR=%ERRORLEVEL%).
exit %ERRORLEVEL%
