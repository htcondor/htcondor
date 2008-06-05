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

REM Build Condor from a batch file
REM Todd Tannenbaum <tannenba@cs.wisc.edu> Feb 2002

REM We want to be able to make the build exit with an exit code
REM instead of setting ERRORLEVEL, if, say, we're calling the bat file
REM from Perl, which doesn't understand ERRORLEVEL.
set INTERACTIVE=/b
IF "%1" == "/exit" set INTERACTIVE=

call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

REM Build the externals
call make_win32_externals.bat
if %ERRORLEVEL% NEQ 0 goto failure

REM Copy any .dll files created by the externals in debug and release
call copy_external_dlls.bat
if %ERRORLEVEL% NEQ 0 goto failure

REM Configure the environment
call set_vars.bat
if %ERRORLEVEL% NEQ 0 goto failure

set CONFIGURATION=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (
    set CONFIGURATION=Debug 
    shift 
    echo Debug Build - Output going to ..\Debug
) else (
    echo Release Build - Output going to ..\Release
)

call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

REM Generate the SOAP API
echo Building gsoap...
nmake /NOLOGO /f gsoap.mak || goto failure

REM Build condor (build order is now preserved in project)
echo Building Condor...
msbuild condor.sln /nologo /t:condor /p:Configuration=%CONFIGURATION% || goto failure 

echo.
echo *** Done.  Build is all happy.  Congrats!  Go drink beer.  
exit %INTERACTIVE% 0
:failure
echo.
echo *** Build Stopped.  Please fix errors and try again.
exit %INTERACTIVE% 1
