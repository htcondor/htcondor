@echo off
if "~%1"=="~EXTERNALS" (
   @echo TODO: separate EXTERNALS build target from ALL_BUILD
   exit /B 0
) else if NOT "~%1"=="~" (
   @echo target=%1
)

@echo CD=%CD%
@echo HOME=%HOME%
@echo CONDOR_BLD_EXTERNAL_STAGE=%CONDOR_BLD_EXTERNAL_STAGE%
@echo LIB=%LIB%
@echo INCLUDE=%INCLUDE%
@echo PATH=%PATH%

REM use FOR to convert from linux path separators to windows path seps
for %%I in ("%BASE_DIR%") do set BUILD_ROOT=%%~dpfI
if "~%BUILD_ROOT%"=="~" set BUILD_ROOT=%CD%
@echo BUILD_ROOT=%BUILD_ROOT%

md %BUILD_ROOT%\Temp
REM pcre blows up if the temp path has spaces in it, so make sure that it's a short path.
set TEMP=%BUILD_ROOT%\Temp
set TMP=%BUILD_ROOT%\Temp

for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI
set VS_DIR=%VS90ROOT:~0,-1%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VS_DIR%\bin

set DOTNET_PATH=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727

set PERL_PATH=
:: Is active perl passed as prereqs?
set ACTIVE_PERL_DIR=%_NMI_PREREQ_ActivePerl_ROOT%
echo nmi ACTIVE_PERL_DIR=%ACTIVE_PERL_DIR%
if NOT "~%ACTIVE_PERL_DIR%"=="~" goto got_active_perl
:: Look for active perl in the registry
for /F "tokens=3" %%I in ('reg query HKLM\Software\Perl /v BinDir') do set ACTIVE_PERL_DIR=%%~sdpI
if NOT "~%ACTIVE_PERL_DIR%"=="~" for %%I in (%ACTIVE_PERL_DIR%\..) do set ACTIVE_PERL_DIR=%%~sfI
echo reg ACTIVE_PERL_DIR=%ACTIVE_PERL_DIR%
if NOT "~%ACTIVE_PERL_DIR%"=="~" goto got_active_perl
:: look for perl in the path. this is dangerous, because we can't use cygwin perl. builds require active perl
for %%I in (perl.exe) do set ACTIVE_PERL_DIR=%%~sdp$PATH:I
if NOT "~%ACTIVE_PERL_DIR%"=="~" for %%I in (%ACTIVE_PERL_DIR%\..) do set ACTIVE_PERL_DIR=%%~sfI
echo path ACTIVE_PERL_DIR=%ACTIVE_PERL_DIR%
if NOT "~%ACTIVE_PERL_DIR%"=="~" goto got_active_perl
set PERL_PATH=c:\perl\site\bin;c:\perl\bin
:got_active_perl
:: strip trailing \ from active perl dir, and then construct a PERL_PATH from it
if "~%ACTIVE_PERL_DIR:~-1%"=="~\" set ACTIVE_PERL_DIR=%ACTIVE_PERL_DIR:~0,-1%
if NOT "~%ACTIVE_PERL_DIR%"=="~" set PERL_PATH=%ACTIVE_PERL_DIR%\site\bin;%ACTIVE_PERL_DIR%\bin;%PERL_PATH%
echo PERL_PATH=%PERL_PATH%
:got_perl

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

if not "~%1"=="~" goto %1
:DEFAULT
@echo devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
goto finis

:ALL_BUILD
:BUILD
@echo cmake.exe . -G "Visual Studio 9 2008"
cmake.exe . -G "Visual Studio 9 2008"
if ERRORLEVEL 1 goto finis
@echo devenv CONDOR.sln /Build RelWithDebInfo /project ALL_BUILD
devenv CONDOR.sln /Build RelWithDebInfo /project ALL_BUILD
if ERRORLEVEL 1 goto finis
:RELEASE
@echo cmake.exe -DBUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=release_dir -P cmake_install.cmake
cmake.exe -DBUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=release_dir -P cmake_install.cmake
goto finis
   
:ZIP
@echo zipping up release directory %BUILD_ROOT%\release_dir
@echo on
pushd %BUILD_ROOT%\release_dir
izip -r ..\condor-%2winnt-x86.zip *
dir .
popd
@echo off
goto finis   
   
:MSI
:MAKE_MSI
:NATIVE
@echo %BUILD_ROOT%\release_dir\etc\WiX\do_wix %BUILD_ROOT\release_dir %BUILD_ROOT\condor-%2winnt-x86.msi
@echo TODO: fix so that do_wix.bat can run in NMI. %ERRORLEVEL%
@echo on
dir %BUILD_ROOT%\release_dir
dir %BUILD_ROOT%
@echo off
:: reset set errorlevel to 0
verify >NUL
:: call %BUILD_ROOT%\release_dir\etc\WiX\do_wix.bat %BUILD_ROOT\release_dir %BUILD_ROOT\condor-%2winnt-x86.msi
@echo ERRORLEVEL=%ERRORLEVEL%
goto finis

:PACK
@echo devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
goto finis

:EXTERNALS
@echo devenv CONDOR_EXTERNALS.sln /Build RelWithDebInfo /project ALL_BUILD
devenv CONDOR_EXTERNALS.sln /Build RelWithDebInfo /project ALL_BUILD
goto finis

REM common exit
:finis
REM if the build failed, we don't want to continue, just exit the cmd shell and return the error
REM if we are in NMI, then we want to quit the command shell and not just the batch file.
if NOT "~%NMI_PLATFORM_TYPE%"=="~nmi" exit /B %ERRORLEVEL%
if ERRORLEVEL 1 exit %ERRORLEVEL%
