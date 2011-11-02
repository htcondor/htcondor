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

:: pick up compiler path from VS90COMNTOOLS environment variable
::
for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI
set VS_DIR=%VS90ROOT:~0,-1%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VS_DIR%\bin

set DOTNET_PATH=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727

:: figure out path to active state perl.  It's different between old batlab and new batlab
::
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

:: figure out the path to 7-Zip
::
for %%I in (7z.exe) do set ZIP_PATH=%%~sdp$PATH:I
echo path ZIP_PATH=%ZIP_PATH%
if NOT "~%_NMI_PREREQ_7_Zip_ROOT%"=="~" (
   set ZIP_PATH=%_NMI_PREREQ_7_Zip_ROOT%
) else (
   if "~%ZIP_PATH%"=="~" (
      set ZIP_PATH=%ProgramFiles%\7-Zip
      echo guess ZIP_PATH=%ZIP_PATH%
   )
)
:: strip trailing \ from zip dir
if "~%ZIP_PATH:~-1%"=="~\" set ZIP_PATH=%ZIP_PATH:~0,-1%

:: figure out where the cmake bin directory is.
::
for %%I in (cmake.exe) do set CMAKE_BIN_DIR=%%~sdp$PATH:I
echo path CMAKE_BIN_DIR=%CMAKE_BIN_DIR%
if NOT "~%_NMI_PREREQ_cmake_ROOT%"=="~" (
   set CMAKE_BIN_DIR=%_NMI_PREREQ_cmake_ROOT%\bin
   echo nmi CMAKE_BIN_DIR=%CMAKE_BIN_DIR%
) else (
   if "~%CMAKE_BIN_DIR%"=="~" (
      set CMAKE_BIN_DIR=C:\Program Files\CMake 2.8\bin
      echo guess CMAKE_BIN_DIR=%CMAKE_BIN_DIR%
   )
)
:: strip trailing \ from cmake bin dir
if "~%CMAKE_BIN_DIR:~-1%"=="~\" set CMAKE_BIN_DIR=%CMAKE_BIN_DIR:~0,-1%

:: set path to WIX binaries
set WIX_PATH=%WIX%
if "~%WIX_PATH:~-1%"=="~\" set WIX_PATH=%WIX_PATH:~0,-1%
if NOT "~%WIX_PATH%"=="~" set WIX_PATH=%WIX_PATH%\bin

:: set path to MSCONFIG binaries
set MSCONFIG_TOOLS_DIR=%BUILD_ROOT%\msconfig

set PATH=%SystemRoot%\system32;%SystemRoot%;%PERL_PATH%;%MSCONFIG_TOOLS_DIR%;%VS_DIR%\Common7\IDE;%VC_BIN%;%CMAKE_BIN_DIR%;%ZIP_PATH%;%WIX_PATH%
@echo PATH=%PATH%

set INCLUDE=%BUILD_ROOT%\src\condor_utils
@echo INCLUDE=%INCLUDE%

:: pick condor version out of cmake files
if NOT "~%2"=="~" (
   set BUILD_VERSION=%2
) else (
   for /f "tokens=2 delims=) " %%I in ('grep set.VERSION CMakeLists.txt') do SET BUILD_VERSION=%%~I
)

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
@echo the time is:
time /t
@echo experimental touching...
dir CMakeLists.txt
dir CMakeFiles\generate.stamp*
for /F %%I in ('dir /b/s CMakeLists.*') do touch %%I
dir CMakeLists.txt
dir CMakeFiles\generate.stamp*
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
@echo ZIPPING up build logs
:: zip build products before zip the release directory so we don't include condor zip file build_products
izip -r build_products.zip * -i *.cmake -i *.txt -i *.htm -i *.map -i *.vcproj -i *.sln -i *.log -i *.stamp* -i param_info* 
@echo ZIPPING up release directory %BUILD_ROOT%\release_dir
pushd %BUILD_ROOT%\release_dir
izip -r ..\condor-%BUILD_VERSION%-winnt-x86.zip *
dir .
popd
goto finis   
   
:ZIP_ALL
@echo ZIPPING up ALL build products
:: zip build products before zip the release directory so we don't include condor zip file build_products
dir
izip -r -q build_products.zip  bld_external build CMakeFiles doc externals nmi_tools soar src -x condor*.zip
izip -D -q build_products.zip *
goto finis   

:ZIP_EXTERNALS
@echo ZIPPING up externals from bld_external and externals directories
:: zip build products before zip the release directory so we don't include condor zip file build_products
dir bld_external
izip -r build_externals.zip  bld_external externals
goto finis   

:MSI
:MAKE_MSI
:NATIVE
@echo %BUILD_ROOT%\release_dir\etc\WiX\do_wix %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-winnt-x86.msi
@echo TODO: fix so that do_wix.bat can run in NMI. %ERRORLEVEL%
@echo on
dir %BUILD_ROOT%\release_dir
dir %BUILD_ROOT%
@echo off
:: reset set errorlevel to 0
verify >NUL
call %BUILD_ROOT%\release_dir\etc\WiX\do_wix.bat %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-winnt-x86.msi
@echo ERRORLEVEL=%ERRORLEVEL%
:: reset set errorlevel to 0
verify >NUL
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
