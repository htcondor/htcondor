@echo off
REM Build Classads from a batch file

setlocal

REM Here's where you specify where your STL header files live
set STL_INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\Include\stlport

if defined INCLUDE goto :check_sdk
call VCVARS32.BAT
if defined INCLUDE goto :check_sdk
call "C:\Program Files\Microsoft Visual Studio\VC98\bin\vcvars32.bat"
if defined INCLUDE goto :check_sdk
echo *** Visual C++ bin directory not in the path, or compiler not installed.
goto failure
:check_sdk
if defined MSSDK goto :compiler_ready
call setenv.bat /2000 /RETAIL
if defined MSSDK goto :compiler_ready
call "C:\Program Files\Microsoft Platform SDK for Windows XP SP2\setenv.bat" /2000 /RETAIL
if defined MSSDK goto :compiler_ready
echo *** Microsoft SDK directory not in the path, or not installed.
goto failure
:compiler_ready
set INCLUDE=%STL_INCLUDE%;%INCLUDE%
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug& shift & echo Debug Build - Output going to ..\Debug) else (echo Release Build - Output going to ..\Release)

for %%f in ( classad_lib classad_functional_tester classad_unit_tester ) do ( echo *** Building %%f & echo . & nmake /C /f %%f.mak RECURSE="0" CFG="%%f - Win32 %conf%" || goto failure )

endlocal

echo .
echo *** Done.  Build was successful.
exit /B 0
:failure
echo .
echo *** Build Stopped.  Please fix errors and try again.
exit /B 1
