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

:: find msbuild and strip trailing \  (if any) from the path 
::
for /F "tokens=2*" %%I in ('reg query HKLM\Software\Microsoft\MSBuild\Toolsversions\14.0 /v msbuildtoolspath') do set MSBUILD_PATH=%%~sfJ
@echo raw MSBUILD_PATH=%MSBUILD_PATH%
if "~%MSBUILD_PATH%"=="~" goto :no_msbuild
if "~%MSBUILD_PATH:~-1%"=="~\" set MSBUILD_PATH=%MSBUILD_PATH:~0,-1%
::if "%~3"=="x64" goto :show_msbuild
if "%MSBUILD_PATH:~-6%"=="\amd64" set MSBUILD_PATH=%MSBUILD_PATH:~0,-6%
:show_msbuild
@echo MSBUILD_PATH=%MSBUILD_PATH%
:no_msbuild

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

:: pick up compiler path from VS120COMNTOOLS environment variable
::
for /D %%I in ("%VS120COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC120_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS120COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC120_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS120COMNTOOLS%..") do set VS120ROOT=%%~sdpI

:: pick up compiler path from VS140COMNTOOLS environment variable
::
for /D %%I in ("%VS140COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC140_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS140COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC140_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS140COMNTOOLS%..") do set VS140ROOT=%%~sdpI

:: pick up compiler path from VS150COMNTOOLS environment variable
::
for /D %%I in ("%VS150COMNTOOLS%..") do if exist %%~sdpIVC\bin\cl.exe set VC150_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS150COMNTOOLS%..") do if exist %%~sdpICommon7\IDE\devenv.exe set VC150_IDE=%%~sdpICommon7\IDE
for /D %%I in ("%VS150COMNTOOLS%..") do set VS150ROOT=%%~sdpI

:: pick up compiler path from VS170COMNTOOLS environment variable
::
::for /D %%I in ("%VS170COMNTOOLS%..\..") do if exist %%~sdpIVC\bin\cl.exe set VC170_BIN=%%~sdpIVC\bin
for /D %%I in ("%VS170COMNTOOLS%..\Common7") do if exist %%~sdpIIDE\devenv.exe set VC170_IDE=%%~sdpIIDE
for /D %%I in ("%VS170COMNTOOLS%..") do set VS170ROOT=%%~sdpI

if NOT DEFINED VS150ROOT set VS150ROOT=%VS170ROOT%
if NOT DEFINED VS160ROOT set VS160ROOT=%VS170ROOT%

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
if "%~2"=="VC12" (
    if DEFINED VS120ROOT (
        set VS_DIR=%VS120ROOT:~0,-1%
        set VS_GEN="Visual Studio 12 2013"
    )
)
if "%~2"=="VC14" (
    if DEFINED VS140ROOT (
        set VS_DIR=%VS140ROOT:~0,-1%
        set VS_GEN="Visual Studio 14 2015"
    )
)
if "%~2"=="VC15" (
    if DEFINED VS150ROOT (
        set VS_DIR=%VS150ROOT:~0,-1%
        set VS_GEN="Visual Studio 15 2017"
    )
)
:: append Win64 if needed, note we have to strip the quotes from the VS_GEN value before we append to it.
if "%~3"=="x64" set VS_GEN="%VS_GEN:~1,-1% Win64"
:: starting with VC16, we no longer append Win64
if "%~2"=="VC16" (
    if DEFINED VS160ROOT (
        set VS_DIR=%VS160ROOT:~0,-1%
        set VS_GEN="Visual Studio 16 2019"
    )
)
if "%~2"=="VC17" (
    if DEFINED VS170ROOT (
        set VS_DIR=%VS170ROOT:~0,-1%
        set VS_GEN="Visual Studio 17 2022"
    )
)
echo VS_DIR is now [%VS_DIR%] %VS_GEN%
dir "%VS_DIR%"
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VC_DIR%\bin
if exist "%VS_DIR%\Common7\IDE\devenv.exe" set DEVENV_DIR=%VS_DIR%\Common7\IDE

:: Visual Studio 17 2022 comes with a new msbuild
if exist "%VS_DIR%\MSBuild\Current\bin\msbuild.exe" set MSBUILD_PATH=%VS_DIR%\MSBuild\Current\bin
if exist "%VS_DIR%\..\MSBuild\Current\bin\msbuild.exe" set MSBUILD_PATH=%VS_DIR%\..\MSBuild\Current\bin
echo MSBUILD_PATH is now [%MSBUILD_PATH%]

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
:: If building with Visual Studio 14 or later, cmake 3 is required
for /F "tokens=3" %%I in ('%CMAKE_BIN_DIR%cmake.exe -version') do set CMAKE_VER=%%~nI
echo CMAKE_VER=%CMAKE_VER%
if "%~2"=="VC17" goto :need_cmake3
if "%~2"=="VC15" goto :need_cmake3
if "%~2"=="VC14" goto :need_cmake3
goto :clean_cmake
:need_cmake3
if "%CMAKE_VER:~0,1%"=="2" (
    set CMAKE_BIN_DIR=C:\Program Files\CMake3\bin
    if exist C:\Tools\Cmake3\bin\cmake.exe set CMAKE_BIN_DIR=C:\Tools\Cmake3\bin
    for /F "tokens=3" %%I in ('"%CMAKE_BIN_DIR%\cmake.exe" -version') do set CMAKE_VER=%%~nI
)
echo using cmake CMAKE_BIN_DIR=%CMAKE_BIN_DIR%
echo using cmake CMAKE_VER=%CMAKE_VER%

:clean_cmake
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

set PATH=%SystemRoot%\system32;%SystemRoot%;%PERL_PATH%;%MSCONFIG_TOOLS_DIR%;%VC_BIN%;%CMAKE_BIN_DIR%
if NOT "~%DEVENV_DIR%"=="~" set PATH=%PATH%;%DEVENV_DIR%
if NOT "~%MSBUILD_PATH%"=="~" set PATH=%MSBUILD_PATH%;%PATH%
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
if "%BUILDID:~0,2%"=="VC" set BUILDID=%4
if "%BUILDID:~1,1%"=="." (
   set BUILD_VERSION=%BUILDID%
   set BUILDID=
) else (
   if exist CMakeLists.txt for /f "tokens=2 delims=) " %%I in ('grep -i set.VERSION CMakeLists.txt') do set BUILD_VERSION=%%~I
   if exist CPackConfig.cmake for /f "tokens=2 delims=) " %%I in ('grep -i "set.CPACK_PACKAGE_VERSION " CPackConfig.cmake') do set BUILD_VERSION=%%~I
)
if NOT "%BUILD_VERSION%"=="" (
  if NOT "%BUILDID%"=="" set BUILD_VERSION=%BUILD_VERSION%-%BUILDID%
)
:: the BUILD_WIN_TAG is used to indicate the Windows version in the .zip and .msi names
:: 7 indicates that XP is no longer supported, which is currently the case when we build with VC11
set BUILD_WIN_TAG=%NMI_PLATFORM:~14%
if "%NMI_PLATFORM%"=="x86_64_Windows10" set BUILD_WIN_TAG=10
if "%NMI_PLATFORM%"=="x86_64_Windows7" set BUILD_WIN_TAG=7
if "%~2"=="VC9" set BUILD_WIN_TAG=XP
set BUILD_ARCH_TAG=x86
if "%~3"=="x64" set BUILD_ARCH_TAG=x64
@echo BUILDID=%BUILDID%
@echo BUILD_VERSION=%BUILD_VERSION%
@echo BUILD_WIN_TAG=%BUILD_WIN_TAG%
@echo BUILD_ARCH_TAG=%BUILD_ARCH_TAG%

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
where devenv || goto :msb_build
@echo devenv CONDOR.sln /Build RelWithDebInfo /project ALL_BUILD
devenv CONDOR.sln /Build RelWithDebInfo /project ALL_BUILD
if ERRORLEVEL 1 goto finis
:RELEASE
@echo cmake.exe -DBUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=release_dir -P cmake_install.cmake
cmake.exe -DBUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=release_dir -P cmake_install.cmake
goto finis
:msb_build
where msbuild
@echo msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 ALL_BUILD.vcxproj
msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 ALL_BUILD.vcxproj
if ERRORLEVEL 1 goto  finis
goto :RELEASE

:ZIP
@echo ZIPPING up build logs
:: zip build products before zip the release directory so we don't include condor zip file build_products
izip -r build_products.zip * -i *.cmake -i *.txt -i *.htm -i *.map -i *.vcproj -i *.sln -i *.log -i *.stamp* -i param_info* 
@echo ZIPPING up release directory %BUILD_ROOT%\release_dir
pushd %BUILD_ROOT%\release_dir
izip -r ..\condor-%BUILD_VERSION%-Windows%BUILD_WIN_TAG%-%BUILD_ARCH_TAG%.zip *
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
@echo %BUILD_ROOT%\release_dir\etc\WiX\do_wix %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-Windows%BUILD_WIN_TAG%-%BUILD_ARCH_TAG%.msi %BUILD_ARCH_TAG%
::@echo on
::dir %BUILD_ROOT%\release_dir
::dir %BUILD_ROOT%
::@echo off
:: verify forces ERRORLEVEL to 0
verify >NUL
call %BUILD_ROOT%\release_dir\etc\WiX\do_wix.bat %BUILD_ROOT%\release_dir %BUILD_ROOT%\condor-%BUILD_VERSION%-Windows%BUILD_WIN_TAG%-%BUILD_ARCH_TAG%.msi %BUILD_ARCH_TAG%
@echo ERRORLEVEL=%ERRORLEVEL%
:: verify forces ERORLEVEL to 0
verify >NUL
goto finis

:PACK
@echo devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Build RelWithDebInfo /project PACKAGE
goto finis

:EXTERNALS
where devenv || goto :msb_extern
grep -E "# Visual Studio" CONDOR.sln
@echo devenv CONDOR.sln /Build RelWithDebInfo /project ALL_EXTERN
devenv CONDOR.sln /Build RelWithDebInfo /project ALL_EXTERN
goto finis
:msb_extern
where msbuild
dir *.vcxproj
@echo msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 ALL_EXTERN.vcxproj
msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 ALL_EXTERN.vcxproj
goto finis

:BUILD_TESTS
:BLD_TESTS
where devenv || goto :msb_tests
@echo devenv CONDOR.sln /Build RelWithDebInfo /project BLD_TESTS
devenv CONDOR.sln /Build RelWithDebInfo /project BLD_TESTS
goto :COPY_TESTS
:msb_tests
where msbuild
dir src\*.vcxproj
@echo msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 src\BLD_TESTS.vcxproj
msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 src\BLD_TESTS.vcxproj
:COPY_TESTS
move src\condor_tests\RelWithDebInfo\*.exe src\condor_tests
move src\condor_tests\RelWithDebInfo\*.pdb src\condor_tests
::for /F %%I in ('findstr /S /M ADD_TEST CTestTestfile.cmake') do move %%~pIRelWithDebInfo\*.exe src\condor_tests
move src\classad\RelWithDebInfo\*.exe src\condor_tests
move src\classad\RelWithDebInfo\*.pdb src\condor_tests
::move src\classad\tests\RelWithDebInfo\*.exe src\condor_tests
::move src\classad\tests\RelWithDebInfo\*.pdb src\condor_tests
::move src\condor_utils\tests\RelWithDebInfo\*.exe src\condor_tests
::move src\condor_utils\tests\RelWithDebInfo\*.pdb src\condor_tests
::move src\condor_collector.V6\tests\RelWithDebInfo\*.exe src\condor_tests
::move src\condor_collector.V6\tests\RelWithDebInfo\*.pdb src\condor_tests
for /f %%I in ('dir /b/s bld_external\pcre.dll') do copy %%~fI src\condor_tests
goto finis

:TAR_TESTS
where devenv || goto :msb_tar_tests
@echo devenv CONDOR.sln /Build RelWithDebInfo /project tests-tar-pkg
devenv CONDOR.sln /Build RelWithDebInfo /project tests-tar-pkg
goto :finis
:msb_tar_tests
@echo msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 src\tests-tar-pkg.vcxproj
msbuild /m:4 /p:Configuration=RelWithDebInfo /fl1 src\tests-tar-pkg.vcxproj
goto :finis

REM common exit
:finis
REM if the build failed, we don't want to continue, just exit the cmd shell and return the error
REM if we are in NMI, then we want to quit the command shell and not just the batch file.
if NOT "~%NMI_PLATFORM_TYPE%"=="~nmi" exit /B %ERRORLEVEL%
if ERRORLEVEL 1 exit %ERRORLEVEL%
