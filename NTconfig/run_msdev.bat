@echo off
REM Prepare to build Condor.
call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

REM Build the externals and bail if it fails.
call make_win32_externals.bat

nmake /f gsoap.mak

REM make_win32_externals implicitly calls set_vars.bat, so just run
REM dev studio as long as the extenals build ok.

if not gsoap%ERRORLEVEL% == gsoap0 goto failure
msdev /useenv condor.dsw 
goto success

:failure
echo *** gsoap stub generator failed ***
exit /b 1

:success


