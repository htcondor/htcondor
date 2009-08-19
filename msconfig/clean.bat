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
REM A simple command line way to clean up the builds and externals
REM Usage: clean [type], where type can be: release, debug or externals
REM ======================================================================

REM Configure the environment (for %EXTERN_DIR%)
call set_vars.bat

REM Determine which build to clean
set CONFIGURATION=Release
if /i A%1==Arelease shift
if /i A%1==Adebug ( 
    set CONFIGURATION=Debug
    shift
)
if /i A%1==Aexternals ( 
    call :clean_externals
    goto :EOF
)

call :clean_build
call :clean_dynamic_configuration
goto :EOF

REM ======================================================================
:clean_build
REM ======================================================================
REM Cleans out the contents of a build directory
REM ======================================================================

echo Cleaning the "%CONFIGURATION%" build and logs.

REM Do the actual cleaning and remove any residual log files
rm -rf ..\%CONFIGURATION%

REM Return to caller
goto :EOF

REM ======================================================================
:clean_externals
REM ======================================================================
REM Cleans out the externals
REM ======================================================================

if A%EXTERN_DIR%==A goto :EOF

echo Cleaning the "Externals".

REM Remove all the "externals" build files
rm -rf %EXTERN_DIR%\build
rm -rf %EXTERN_DIR%\install
rm -rf %EXTERN_DIR%\triggers

REM Return to caller
goto :EOF

REM ======================================================================
:clean_dynamic_configuration
REM ======================================================================
REM Cleans out the externals
REM ======================================================================

echo Cleaning the dynamically generated files.

REM Remove all the generated files
rm -f ..\src\h\syscall_numbers.h
rm -f ..\src\condor_includes\config.h
rm -f ..\src\condor_c++_util\param_info_init.c

REM Return to caller
goto :EOF
