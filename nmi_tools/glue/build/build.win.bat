@echo off
if "~%1"=="~EXTERNALS" (
   ::@echo TODO: separate EXTERNALS build target from ALL_BUILD
   ::exit /B 0
   @echo target=%1
) else if NOT "~%1"=="~" (
   @echo target=%1
)


@echo CD=%CD%
@echo HOME=%HOME%
@echo CONDOR_BLD_EXTERNAL_STAGE=%CONDOR_BLD_EXTERNAL_STAGE%
@echo LIB=%LIB%
@echo INCLUDE=%INCLUDE%
@echo PATH=%PATH%
@echo ARGS=%*

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
for /D %%I in ("%VS90COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC90_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS90COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC90_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI

:: pick up vs2010 compiler path from VS100COMNTOOLS environment variable
::
for /D %%I in ("%VS100COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC100_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS100COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC100_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS100COMNTOOLS%..") do set VS100ROOT=%%~sdpI

:: pick up vs2012 compiler path from VS110COMNTOOLS environment variable
::
for /D %%I in ("%VS110COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC110_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS110COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC110_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS110COMNTOOLS%..") do set VS110ROOT=%%~sdpI

set VS_DIR=%VS90ROOT:~0,-1%
set VS_GEN="Visual Studio 9 2008"
if "%~2"=="VC10" (
    if DEFINED VS100ROOT (
        set VS_DIR=%VS100ROOT:~0,-1%
        set VS_GEN="Visual Studio 10"
    )
)
if "%~2"=="VC11" (
    if DEFINED VS110ROOT (
        set VS_DIR=%VS110ROOT:~0,-1%
        set VS_GEN="Visual Studio 11"
    )
)
echo VS_DIR is now [%VS_DIR%] %VS_GEN%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VC_DIR%\bin


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
if "~%WIX%"=="~" goto no_wix
set WIX_PATH=%WIX%
if "~%WIX_PATH:~-1%"=="~\" set WIX_PATH=%WIX_PATH:~0,-1%
if NOT "~%WIX_PATH%"=="~" set WIX_PATH=%WIX_PATH%\bin
:no_wix

:: set path to MSCONFIG binaries
set MSCONFIG_TOOLS_DIR=%BUILD_ROOT%\msconfig

set PATH=%SystemRoot%\system32;%SystemRoot%;%PERL_PATH%;%MSCONFIG_TOOLS_DIR%;%VS_DIR%\Common7\IDE;%VC_BIN%;%CMAKE_BIN_DIR%
if NOT "~%ZIP_PATH%"=="~" set PATH=%PATH%;%ZIP_PATH%
if NOT "~%WIX_PATH%"=="~" set PATH=%PATH%;%WIX_PATH%
@echo PATH=%PATH%

set INCLUDE=%BUILD_ROOT%\src\condor_utils
@echo INCLUDE=%INCLUDE%

:: the condor version or build id may be passed as arg 2
:: %2 is either blank, a full version number (X.Y.Z) or the build id
:: if its a full version number than use it. (we look for the "." after X)
:: if it's a buildid, get the version number from the cmake files and then append it.
set BUILDID=%2
if "%BUILDID:~0,2%"=="VC" set BUILDID=%3
if "%BUILDID:~1,1%"=="." (
   set BUILD_VERSION=%BUILDID%
   set BUILDID=
) else (
   for /f "tokens=2 delims=) " %%I in ('grep set.VERSION CMakeLists.txt') do set BUILD_VERSION=%%~I
)
if NOT "%BUILD_VERSION%"=="" (
  if NOT "%BUILDID%"=="" set BUILD_VERSION=%BUILD_VERSION%-%BUILDID%
)
@echo BUILDID=%BUILDID%
@echo BUILD_VERSION=%BUILD_VERSION%

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
:: the Windows VM's have trouble with time sync, so touch all of the files to makes sure that they build.
@echo the time is:
time /t
@echo experimental touching...
dir CMakeLists.txt
dir CMakeFiles\generate.stamp*
for /F %%I in ('dir /b/s CMakeLists.*') do touch %%I    
dir CMakeLists.txt
dir CMakeFiles\generate.stamp*
@echo cmake.exe . -G %VS_GEN%
cmake.exe . -G %VS_GEN%
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
izip -r ..\condor-%BUILD_VERSION%-Windows-x86.zip *
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
@echo %BUILD_ROOT%\release_dir\etc\WiX\do_wix %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-Windows-x86.msi
::@echo on
::dir %BUILD_ROOT%\release_dir
::dir %BUILD_ROOT%
::@echo off
:: verify forces ERRORLEVEL to 0
verify >NUL
call %BUILD_ROOT%\release_dir\etc\WiX\do_wix.bat %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-Windows-x86.msi
@echo ERRORLEVEL=%ERRORLEVEL%
:: verify forces ERORLEVEL to 0
verify >NUL
goto finis

:PACK
@echo devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
goto finis

:EXTERNALS
::where devenv
::grep -E "# Visual Studio" CONDOR.sln
@echo devenv CONDOR.sln /Build RelWithDebInfo /project ALL_EXTERN
devenv CONDOR.sln /Build RelWithDebInfo /project ALL_EXTERN
goto finis

:BUILD_TESTS
:BLD_TESTS
@echo devenv CONDOR.sln /Build RelWithDebInfo /project BLD_TESTS
devenv CONDOR.sln /Build RelWithDebInfo /project BLD_TESTS
move src\condor_tests\RelWithDebInfo\*.exe src\condor_tests
move src\condor_tests\RelWithDebInfo\*.pdb src\condor_tests
goto finis

REM common exit
:finis
REM if the build failed, we don't want to continue, just exit the cmd shell and return the error
REM if we are in NMI, then we want to quit the command shell and not just the batch file.
if NOT "~%NMI_PLATFORM_TYPE%"=="~nmi" exit /B %ERRORLEVEL%
if ERRORLEVEL 1 exit %ERRORLEVEL%
