@echo on
setlocal enableextensions enabledelayedexpansion

REM note this script only works if it is in the parent directory
REM of the wix xml files.
REM

if "%~1"=="" goto usage
if "%~2"=="" goto usage

set _ARCH_ARG=-arch x64
if "%~3"=="x64" set _ARCH_ARG=-arch x64
if "%~3"=="x86" set _ARCH_ARG=

set RUNEVAL=-sval
if "%USERNAME%" == "" goto noruneval
if not "%_CONDOR_SLOT%" == "" goto noruneval
set RUNEVAL=
:noruneval

set _condor_path=%~sdf1
pushd %~sdp0

set _WXS_FILES=
for %%I in (xml\*.wxs) do set _WXS_FILES=!_WXS_FILES! %%I
rem echo %_WXS_FILES%
rem echo %_WXS_FILES:.wxs=.wixobj%
set _WIXOBJ_FILES=%_WXS_FILES:xml\=%

heat dir %_condor_path% -ke -g1 -srd -sreg -gg -var var.Source -t xml\condor.xsl -out "%~n2.wxs"

candle -ext WixFirewallExtension %_ARCH_ARG% -dSource=%_condor_path% "%~n2.wxs" %_WXS_FILES%

light %RUNEVAL% -ext WixUIExtension -ext WixFirewallExtension -dWixUILicenseRtf=.\license.rtf -out "%2" "%~n2.wixobj" %_WIXOBJ_FILES:.wxs=.wixobj%
popd
goto finis

:usage
@echo USAGE: %0 {condor_path} {msi_fullpath} [x86^|x64]
@echo  {condor_path} is the location of a properly laid out condor installation.
@echo  This can created by unzipping Condor-X.Y.Z.zip to {condor_path}.
@echo  .
@echo  {msi_fullpath} is the full_path of the output package file
@echo  for example {msi_fullpath} might be c:\scratch\temp\condor-7.5.5.msi
@echo  .
@echo  The optional last argument indicates the platform architecture, x64 is the default
:finis
