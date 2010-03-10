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
REM  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implie
REM  See the License for the specific language governing permissions and
REM  limitations under the License.
REM
REM ======================================================================

REM ======================================================================
REM ======================================================================
REM Main entry point
REM ======================================================================
REM ======================================================================

if exist %1 goto process1
echo Error - directory "%1" does not exist
echo Usage: dopackaging.bat path_to_release_subdirectory [output_path]
goto :EOF
:process1
Echo Building installer...
rem %~f1 is the canonical (full) path given in %1
set CONDORRELEASEDIR=%~f1

REM Set and create the ouput directory
if /i A%2==A ( 
    set CONDOROUTPUTDIR=%cd%\..\public 
) else ( 
    set CONDOROUTPUTDIR=%2 
)
echo Creating output directory %CONDOROUTPUTDIR%
mkdir %CONDOROUTPUTDIR%

rem Set up environment
call set_vars.bat

rem REMOVE Cygwin from the path, if needed. This is so gmake will
rem use cmd.exe instead of sh.exe to run shell commands.
set PATH_SAVE=%PATH%
set PATH_ASS=%PATH:c:\cygwin\bin;=%

REM Remove all TRAILING BACKSLASHES from paths listed in PATH.
REM gmake really doesn't like those. (sigh)
set PATH_ASS=%PATH_ASS:\;=;%
set PATH=%PATH_ASS%;%cd%

REM Create the MSI package
pushd installer
gmake -e
popd
if not A%ERRORLEVEL%==A0 (exit /b 1)
set PATH=%PATH_SAVE%

REM Finally, make the zipfile. 
REM CD into the releasedir first, so the zip file does not store
REM the full path to the releasedir.
set CONFIGDIR=%CD%
pushd %CONDORRELEASEDIR%
%CONFIGDIR%\izip -9 -r condor.zip *.*
move /y condor.zip %CONDOROUTPUTDIR%

REM Rename the zip file to match that of the msi. This assumes
REM that there is one and only one *.msi file in the destination
REM subdirectory.  If it isn't, the behaviour is undefined.
REM (%%~nf expands %1 to a file name with no extension)
pushd %CONDOROUTPUTDIR%
for %%f in (*.msi) do move /y condor.zip %%~nf.zip
popd

echo. & echo *** Done. MSI package created. Congrats! Go drink absinthe.
