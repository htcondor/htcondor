@echo CD=%CD%
@echo HOME=%HOME%
@echo LIB=%LIB%
@echo INCLUDE=%INCLUDE%
@echo PATH=%PATH%

for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI
set VS_DIR=%VS90ROOT:~0,-1%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VS_DIR%\bin
set DOTNET_PATH=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727
set PERL_PATH=c:\prereq\ActivePerl-5.10.1\bin;c:\perl\bin
set ZIP_PATH=%ProgramFiles%\7-Zip
set WIX_PATH=%WIX%
set MSCONFIG_TOOLS_DIR=%HOME%\userdir\msconfig
set CMAKE_BIN=%ProgramFiles%\CMake 2.8\bin

set PATH=%SystemRoot%\system32;%SystemRoot%;%PERL_PATH%;%MSCONFIG_TOOLS_DIR%;%VS_DIR%\Common7\IDE;%VC_BIN%;%CMAKE_BIN%;%ZIP_PATH%;%WIX_PATH%
@echo PATH=%PATH%

@echo ----  build.win.bat ENVIRONMENT --------------------------------
set
@echo ----  end build.win.bat ENVIRONMENT ----------------------------

@echo devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
devenv CONDOR.sln /Rebuild RelWithDebInfo /project PACKAGE
