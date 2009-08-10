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

REM Generate the syscall header
call :GENERATE_SYSCALL_NUMBERS
if %ERRORLEVEL% NEQ 0 goto :FAIL

REM Create the log directories
call :CREATE_BUILD_DIR
if %ERRORLEVEL% NEQ 0 goto :EXTERNALS_FAIL

REM Build the externals
call :BUILD_EXTERNALS
if %ERRORLEVEL% NEQ 0 goto :EXTERNALS_FAIL

REM Put our config.h file in the right place
call configure.bat
if %ERRORLEVEL% NEQ 0 goto :CONFIG_FAIL

REM Copy the correct default library vsprops file into place. This 
REM changes which libraries are inlcuded by default into projects
call correct_libs.bat noinit >NUL
if %ERRORLEVEL% NEQ 2 call correct_libs.bat noinit >NUL

REM Make gsoap stubs, etc.
call :MAKE_GSOAP
if %ERRORLEVEL% NEQ 0 goto :GSOAP_FAIL

REM Make param stubs, etc.
call :MAKE_PARAM
if %ERRORLEVEL% NEQ 0 goto :PARAM_FAIL

REM ======================================================================
REM NOTE: make_win32_externals.bat implicitly calls set_vars.bat, so just 
REM run dev studio as long as the extenals build ok.
REM ======================================================================

REM Launch the Visual Studio IDE
call :LAUCH_IDE
if %ERRORLEVEL% NEQ 0 goto :IDE_FAIL

REM We're done, let's get out of here
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
exit /b 1
:EXTERNALS_FAIL
echo *** Failed to build externals ***
exit /b 1
:CONFIG_FAIL
echo *** Failed to make config.h ***
exit /b 1
:GSOAP_FAIL
echo *** gsoap stub generator failed ***
exit /b 1
:PARAM_FAIL
echo *** param stub generator failed ***
exit /b 1
:IDE_FAIL
echo *** Visual Studio IDE launch failed ***
exit /b 1

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
:CREATE_BUILD_DIR
REM ======================================================================
REM Create the log directory
REM ======================================================================
if not exist ..\Release\NUL mkdir ..\Release
if not exist ..\Release\BuildLogs\NUL (
    echo Creating Release log directory
    mkdir ..\Release\BuildLogs
)
exit /b 0

REM ======================================================================
:BUILD_EXTERNALS
REM ======================================================================
REM Build the externals and copy any .dll files created by the externals 
REM in debug and release
REM ======================================================================
if exist ..\Release\BuildLogs\externals.build.log (
    echo Removing old externals build log
    rm -f ..\Release\BuildLogs\externals.build.log
)
echo Building externals (see externals.build.log for details)
call make_win32_externals.bat >..\Release\BuildLogs\externals.build.log 2>&1
if %ERRORLEVEL% NEQ 0 exit /b 1
call copy_external_dlls.bat >NUL 2>NUL
if %ERRORLEVEL% NEQ 0 exit /b 1
exit /b 0

REM ======================================================================
:MAKE_GSOAP
REM ======================================================================
REM Make gsoap stubs, etc.
REM ======================================================================
if exist ..\Release\BuildLogs\gsoap.build.log (
    echo Removing old gsoap build log
    rm -f ..\Release\BuildLogs\gsoap.build.log
)
echo Building gsoap stubs (see gsoap.build.log for details)
nmake /NOLOGO /f gsoap.mak >..\Release\BuildLogs\gsoap.build.log 2>&1
if %ERRORLEVEL% NEQ 0 exit /b 1 
exit /b 0

:MAKE_PARAM
REM ======================================================================
REM Make param stubs, etc.
REM ======================================================================
if exist ..\Release\BuildLogs\param.build.log (
    echo Removing old param build log
    rm -f ..\Release\BuildLogs\param.build.log
)
echo Building param stubs (see param.build.log for details)
nmake /NOLOGO /f param.mak >..\Release\BuildLogs\param.build.log 2>&1
if %ERRORLEVEL% NEQ 0 exit /b 1 
exit /b 0

REM ======================================================================
:LAUCH_IDE
REM ======================================================================
REM Launch the IDE
REM ======================================================================
echo Launching the IDE
if not exist "%DevEnvDir%\devenv.exe" (
    echo Is Visual Studio Installed?
    exit /b 1 
)
REM Use start command to spawn the dev studio as a seperate process
start /D"%DevEnvDir%" devenv.exe /useenv "%cd%\condor.sln"
exit /b 0

REM Future VC Express support? 
REM else if exist "%DevEnvDir%\VCExpress.exe" (
REM     "%DevEnvDir%\VCExpress.exe" /useenv condor.sln

