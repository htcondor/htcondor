@echo off
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
REM Top level script that executes the building of the externals for Win32.
REM ======================================================================

REM First, set some environment variables.
call set_vars.bat
if %ERRORLEVEL% NEQ 0 (echo . && echo *** Failed to prepare build environment && exit /b 1)

for %%e in ( %EXTERNALS_NEEDED% ) do ( if not exist %EXT_TRIGGERS%\%%e ( perl -w %cd%\..\externals\build_external --extern_src=%cd%\..\externals --extern_build=%EXTERN_DIR% --package_name=%%e --extern_config=%cd% || exit /b 1 ) else echo The %%e external up to date. Done. )

REM now just exit with the return code of the last command
exit /B %ERRORLEVEL%
