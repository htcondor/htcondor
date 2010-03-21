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

REM We want to be able to make the release exit with an exit code instead
REM of setting ERRORLEVEL, if, say, we're calling the bat file from Perl,
REM which doesn't understand ERRORLEVEL.
set INTERACTIVE=/b
if "%1" == "/exit" ( 
    set INTERACTIVE=
    shift
)

REM Check if the output directory exists
if exist %1 goto continue1
echo Error - directory %1 does not exist
echo Usage: dorelease.bat "<release_path>"
goto end
:continue1
REM Check if the build exists
if exist ..\Release goto continue2
echo Compile a Release Build of Condor first.
goto end
:continue2

REM Set up environment
call set_vars.bat

echo Creating Release Directory...
if not exist %1\bin\NUL mkdir %1\bin
if not exist %1\lib\NUL mkdir %1\lib
if not exist %1\lib\webservice\NUL mkdir %1\lib\webservice
if not exist %1\etc\NUL mkdir %1\etc
if not exist %1\sql\NUL mkdir %1\sql
if not exist %1\src\NUL mkdir %1\src
if not exist %1\src\chirp\NUL mkdir %1\src\chirp
if not exist %1\src\drmaa\NUL mkdir %1\src\drmaa
if not exist %1\src\userlog\NUL mkdir %1\src\userlog
if not exist %1\include\NUL mkdir %1\include
if not exist %1\examples\NUL mkdir %1\examples
if not exist %1\examples\cpusoak\NUL mkdir %1\examples\cpusoak
if not exist %1\examples\printname\NUL mkdir %1\examples\printname
if not exist %1\hdfs\NUL mkdir %1\hdfs

echo. & echo Copying root Condor files...
copy ..\Release\*.exe %1\bin
copy ..\Release\*.dll %1\bin
copy msvcrt.dll %1\bin
copy msvcirt.dll %1\bin
copy Microsoft.VC90.CRT.manifest %1\bin
copy msvcm90.dll %1\bin
copy msvcp90.dll %1\bin
copy msvcr90.dll %1\bin
copy mkisofs.bat %1\bin
copy cdmake.exe %1\bin
copy ..\src\condor_vm-gahp\condor_vm_vmware.pl %1\bin

echo. & echo Copying Chirp files...
copy ..\src\condor_starter.V6.1\*.class %1\lib
copy ..\src\condor_starter.V6.1\*.jar %1\lib
copy ..\src\condor_chirp\Chirp.jar %1\lib
copy ..\src\condor_chirp\chirp_* %1\src\chirp
copy ..\src\condor_chirp\PROTOCOL %1\src\chirp
copy ..\src\condor_chirp\chirp\LICENSE %1\src\chirp
copy ..\src\condor_chirp\chirp\doc\Condor %1\src\chirp\README

echo. & echo Copying user log library...
copy "..\src\condor_c++_util\write_user_log.h" %1\src\userlog
copy "..\src\condor_c++_util\read_user_log.h" %1\src\userlog
copy ..\Release\condor_api.lib %1\src\userlog

echo. & echo Copying example configurations...
copy ..\src\condor_examples\condor_config.* %1\etc
copy ..\src\condor_examples\condor_vmware_local_settings %1\etc

echo. & echo Copying example submit files...
copy installer\examples\*.* %1\examples
copy installer\examples\cpusoak\*.* %1\examples\cpusoak
copy installer\examples\printname\*.* %1\examples\printname

echo. & echo Copying SQL files...
copy ..\src\condor_tt\*.sql %1\sql

echo. & echo Copying WSDL files...
pushd ..\src
for /R %%f in (*.wsdl) do copy %%f %1\lib\webservice
popd

echo. & echo Copying symbol files...
for %%f in (master startd quill dbmsd had credd schedd collector negotiator shadow starter) do (
    copy ..\Release\condor_%%f.pdb %1\bin
)

echo. & echo Copying hadoop files...
xcopy ..\externals\install\hdfs %1\hdfs /E

echo. & echo Making some aliases...
pushd %1\bin
copy condor_rm.exe condor_hold.exe
copy condor_rm.exe condor_release.exe
copy condor_rm.exe condor_vacate_job.exe
move condor_tool.exe condor.exe
copy condor.exe condor_on.exe
copy condor.exe condor_off.exe
copy condor.exe condor_restart.exe
copy condor.exe condor_reconfig.exe
copy condor.exe condor_reschedule.exe
copy condor.exe condor_vacate.exe
copy condor.exe condor_set_shutdown.exe
copy condor.exe condor_squawk.exe
copy condor_cod.exe condor_cod_request.exe
popd

echo. & echo Copying DRMAA files...
pushd %EXT_INSTALL%\%EXT_DRMAA_VERSION%
copy include\*.* %1\include
copy lib\*.* %1\lib
copy src\*.* %1\src\drmaa
popd

echo. & echo *** Done. Windows release finished. Congrats! Go drink whiskey.

:end
exit %INTERACTIVE% 0
