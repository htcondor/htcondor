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
REM ======================================================================
REM Main entry point
REM ======================================================================
REM ======================================================================

REM Set the environment if we need to (i.e. when we are called with no
REM params; most likely by the user, wanting to revert the changes)
if /i not A%1==Anoinit call set_vars.bat

REM Shorten the path to make things more legible
set DEFAULTS="%VC_DIR%\VCProjectDefaults"

REM ======================================================================
REM Copy our version of CoreWin.vsprops in to place.  We need to do this
REM because, by default, VS9 adds some default libraries that are neither
REM required to run Condor, nor do the libraries it references necessarily
REM exist in every development environment.
REM ======================================================================
if exist %DEFAULTS%\CoreWin.vsprops.old goto :restore
echo *** Replacing the standard Windows libraries.
move /y %DEFAULTS%\CoreWin.vsprops %DEFAULTS%\CoreWin.vsprops.old >NUL 2>&1
copy /y CondorCoreWin.vsprops %DEFAULTS%\CoreWin.vsprops >NUL 2>&1
exit /b 2
:restore
echo *** Restoring the standard libraries.
move /y %DEFAULTS%\CoreWin.vsprops.old %DEFAULTS%\CoreWin.vsprops >NUL 2>&1
exit /b 1
