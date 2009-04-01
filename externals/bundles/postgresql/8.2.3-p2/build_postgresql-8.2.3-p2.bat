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
REM Manages the building of the postgresql external on Win32
REM Note: On Win32, we only worry about building the postgres client
REM ======================================================================

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%

REM Copy the patch file to the desired location
cp win32.mak %PACKAGE_SRC_NAME%\src\interfaces\libpq

REM Move to the postgresql interface directory
cd %PACKAGE_SRC_NAME%\src\interfaces\libpq

REM Build the client libs
nmake -f win32.mak
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM ** copy output into install dir
mkdir %PACKAGE_INSTALL_DIR%\out32dll
mkdir %PACKAGE_INSTALL_DIR%\inc32

xcopy Release\*.lib %PACKAGE_INSTALL_DIR%\out32dll
if %ERRORLEVEL% NEQ 0 goto :FAIL
xcopy Release\*.dll %PACKAGE_INSTALL_DIR%\out32dll
if %ERRORLEVEL% NEQ 0 goto :FAIL

xcopy libpq-fe.h %PACKAGE_INSTALL_DIR%\inc32
if %ERRORLEVEL% NEQ 0 goto :FAIL

cd ..\..\include

xcopy /s *.* %PACKAGE_INSTALL_DIR%\inc32
if %ERRORLEVEL% NEQ 0 goto :FAIL

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
echo. & echo *** Building %PACKAGE_NAME% failed (ERROR=%ERRORLEVEL%).
exit %ERRORLEVEL%

