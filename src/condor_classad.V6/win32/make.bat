@echo off
REM Build Classads from a batch file

REM Here's where you specify where your STL header files live
set STL_INCLUDE=C:\Program Files\Microsoft Visual Studio\VC98\Include\stlport

if defined INCLUDE goto :compiler_ready
call VCVARS32.BAT
set Include=%STL_INCLUDE%;%Include%
if defined INCLUDE goto :compiler_ready
echo *** Visual C++ bin direcotory not in the path, or compiler not installed.
goto failure
:compiler_ready
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug& shift & echo Debug Build - Output going to ..\Debug) else (echo Release Build - Output going to ..\Release)

for %%f in ( classad_dist ) do ( echo *** Building %%f & echo . & nmake /C /f %%f.mak RECURSE="0" CFG="%%f - Win32 %conf%" || goto failure )

echo .
echo *** Done.  Build was successful.
exit /B 0
:failure
echo .
echo *** Build Stopped.  Please fix errors and try again.
exit /B 1
