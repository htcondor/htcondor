@echo off
setlocal
if exist %1 goto process1
echo Error - directory "%1" does not exist
echo Usage: dopackaging.bat path_to_release_subdirectory [output_path]
goto :EOF
:process1
Echo Building installer...
rem %~f1 is the canonical (full) path given in %1
set CONDORRELEASEDIR=%~f1

if /i A%2==A ( set CONDOROUTPUTDIR=%cd%\..\public ) else ( set CONDOROUTPUTDIR=%2 )

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

pushd .
cd windows_installer
gmake -e
popd

REM Finally, make the zipfile.
if not A%ERRORLEVEL%==A0 (exit /b 1)
set PATH=%PATH_SAVE%
zip -r condor.zip %CONDORRELEASEDIR%
move /y condor.zip %CONDOROUTPUTDIR%

REM rename the zip file to match that of the msi
forfiles -p %CONDOROUTPUTDIR% -m *.MSI -c "cmd /c move /y condor.zip @FNAME.zip"

echo Done.
