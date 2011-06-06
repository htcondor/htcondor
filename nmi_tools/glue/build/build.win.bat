@echo off
@echo CD=%CD%
@echo HOME=%HOME%
@echo CONDOR_BLD_EXTERNAL_STAGE=%CONDOR_BLD_EXTERNAL_STAGE%
@echo LIB=%LIB%
@echo INCLUDE=%INCLUDE%
@echo PATH=%PATH%

REM use FOR to convert from linux path separators to windows path seps
for %%I in ("%BASE_DIR%") do set BUILD_ROOT=%%~dpfI

md %BUILD_ROOT%\Temp
REM pcre blows up if the temp path has spaces in it, so make sure that it's a short path.
set TEMP=%BUILD_ROOT%\Temp
set TMP=%BUILD_ROOT%\Temp

for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI
set VS_DIR=%VS90ROOT:~0,-1%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VS_DIR%\bin

set DOTNET_PATH=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727

set PERL_PATH=c:\perl\site\bin;c:\perl\bin
for /D %%I in ("c:\prereq\ActivePerl*") do set ACTIVE_PERL_DIR=%%~fI
if NOT "~%ACTIVE_PERL_DIR%"=="~" set PERL_PATH=%ACTIVE_PERL_DIR%\site\bin;%ACTIVE_PERL_DIR%\bin;%PERL_PATH%

if "~%_NMI_PREREQ_7_Zip_ROOT%"=="~" (
  set ZIP_PATH=%ProgramFiles%\7-Zip
) else (
  set ZIP_PATH=%_NMI_PREREQ_7_Zip_ROOT%
)
set WIX_PATH=%WIX%
set MSCONFIG_TOOLS_DIR=%BUILD_ROOT%\msconfig
if "~%_NMI_PREREQ_cmake_ROOT%"=="~" (
   set CMAKE_BIN_DIR=%ProgramFiles%\CMake 2.8\bin
) else (
   set CMAKE_BIN_DIR=%_NMI_PREREQ_cmake_ROOT%\bin
)

set PATH=%SystemRoot%\system32;%SystemRoot%;%PERL_PATH%;%MSCONFIG_TOOLS_DIR%;%VS_DIR%\Common7\IDE;%VC_BIN%;%CMAKE_BIN_DIR%;%ZIP_PATH%;%WIX_PATH%
@echo PATH=%PATH%

set INCLUDE=%BUILD_ROOT%\src\condor_utils
@echo INCLUDE=%INCLUDE%

@echo ----  build.win.bat ENVIRONMENT --------------------------------
set
@echo ----  end build.win.bat ENVIRONMENT ----------------------------

@echo devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
REM if the build failed, we don't want to continue, just exit the cmd shell and return the error
REM if we are in NMI, then we want to quit the command shell and not just the batch file.
if NOT "~%NMI_PLATFORM_TYPE%"=="~nmi" exit /B %ERRORLEVEL%
if ERRORLEVEL 1 exit %ERRORLEVEL%
