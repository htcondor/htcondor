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

REM ======================================================================
REM ======================================================================
REM Main entry point
REM ======================================================================
REM ======================================================================

REM We want to be able to make the build exit with an exit code
REM instead of setting ERRORLEVEL, if, say, we're calling the bat file
REM from Perl, which doesn't understand ERRORLEVEL.
set INTERACTIVE=/b
IF "%1" == "/exit" set INTERACTIVE=

call :GENERATE_SYSCALL_NUMBERS
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM Build the externals
call :BUILD_EXTERNALS
if %ERRORLEVEL% NEQ 0 goto :EXTERNALS_FAIL

REM Copy any .dll files created by the externals in debug and release
call copy_external_dlls.bat
if %ERRORLEVEL% NEQ 0 goto :FAIL

call :DETERMINE_CONFIGRATION
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM Put our config.h file in the right place
call configure.bat
if %ERRORLEVEL% NEQ 0 goto :CONFIG_FAIL

REM Make gsoap stubs, etc.
call :MAKE_GSOAP
if %ERRORLEVEL% NEQ 0 goto :GSOAP_FAIL

REM ======================================================================
REM NOTE: make_win32_externals.bat implicitly calls set_vars.bat, so just 
REM run the build as long as the extenals built ok.
REM ======================================================================

REM Launch the Visual Studio IDE
call :RUN_BUILD
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM We're done, let's get out of here
echo *** Done. Build is all happy. Congrats! Go drink beer.
endlocal
goto :EOF

REM ======================================================================
REM ======================================================================
REM Functions
REM ======================================================================
REM ======================================================================

REM ======================================================================
:FAIL
REM ======================================================================
REM All the failure calls
REM ======================================================================
echo *** Build Stopped. Please fix errors and try again.
exit %INTERACTIVE% 1
:EXTERNALS_FAIL
echo *** Failed to build externals ***
exit %INTERACTIVE% 1
:CONFIG_FALL
echo *** Failed to make config.h ***
exit %INTERACTIVE% 1
:GSOAP_FAIL
echo *** gsoap stub generator failed ***
exit %INTERACTIVE% 1

REM ======================================================================
:GENERATE_SYSCALL_NUMBERS
REM ======================================================================
REM Although we have it as a rule in the .dsp files, somehow our prebuild 
REM rule for syscall_numbers.h gets lost into the translation to .mak files, 
REM so we deal with it here explicitly.
REM ======================================================================
if not exist ..\src\h\syscall_numbers.h awk -f ..\src\h\awk_prog.include_file ..\src\h\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h
exit /b 0

REM ======================================================================
:BUILD_EXTERNALS
REM ======================================================================
REM Build the externals and copy any .dll files created by the externals 
REM in debug and release
REM ======================================================================
call make_win32_externals.bat
if %ERRORLEVEL% NEQ 0 exit /b 1
call copy_external_dlls.bat
if %ERRORLEVEL% NEQ 0 exit /b 1
exit /b 0

REM ======================================================================
:DETERMINE_CONFIGRATION
REM ======================================================================
REM Determine the build type
REM ======================================================================
set CONFIGURATION=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (
    set CONFIGURATION=Debug 
    shift 
)
echo *** %CONFIGURATION% Build
exit /b 0

REM ======================================================================
:MAKE_GSOAP
REM ======================================================================
REM Make gsoap stubs, etc.
REM ======================================================================
nmake /NOLOGO /f gsoap.mak
if %ERRORLEVEL% NEQ 0 exit /b 1
exit /b 0

REM ======================================================================
:RUN_BUILD
REM ======================================================================
REM Build condor (build order is now preserved in project)
REM ======================================================================
echo Building Condor...
set
msbuild condor.sln /nologo /t:condor /p:Configuration=%CONFIGURATION%;VCBuildUseEnvironment="true"
REM devenv condor.sln /useenv /build "%CONFIGURATION%"
if %ERRORLEVEL% NEQ 0 exit /b 1
exit /b 0
