@echo off
REM Prepare to build Condor.

REM Keep all environment changes local to this script
setlocal

call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

REM This fixes the wierdness in condor_cpp_util.dsp
awk "{ gsub(/\.\\\.\.\\Debug/, \"..\\Debug\") } { gsub(/\.\\\.\.\\Release/, \"..\\Release\") } { print }" condor_cpp_util.dsp > ~temp.dsp
del condor_cpp_util.dsp
ren ~temp.dsp condor_cpp_util.dsp

REM InputDir is stupidly defined by an absolute path by MSVS. So we
REM have to have awk clean up by making everything relative.
for %%f in ( *.dsp ) do awk "{ b = gensub(/InputDir=(.*)\\src\\(.+)/, \"InputDir=..\\\\src\\\\\\2\", $0); print b }" %%f > %%f.tmp

if exist condor_util_lib.dsp.tmp (
	del *.dsp
	ren *.tmp *.
		) ELSE (
			echo awk makefile cleanup failed!
		   	exit /b 1
		)

REM Although we have it as a rule in the .dsp files, somehow our prebuild 
REM rule for syscall_numbers.h gets lost into the translation to .mak files, 
REM so we deal with it here explicitly.
if not exist ..\src\h\syscall_numbers.h awk -f ..\src\h\awk_prog.include_file ..\src\h\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

REM Build the externals
call make_win32_externals.bat
if not errorlevel 0 goto extfail

REM Copy any .dll files created by the externals in debug and release
call copy_external_dlls.bat
if not errorlevel 0 goto extfail

REM Deal with gsoap
nmake /f gsoap.mak

REM make_win32_externals implicitly calls set_vars.bat, so just run
REM dev studio as long as the extenals build ok.

if not gsoap%ERRORLEVEL% == gsoap0 goto failure
msdev /useenv condor.dsw 
goto success

:failure
echo *** gsoap stub generator failed ***
exit /b 1
:extfail
echo *** Failed to build externals ***
exit /b 1

:success


