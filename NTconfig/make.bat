@echo off
REM Build Condor from a batch file
REM Todd Tannenbaum <tannenba@cs.wisc.edu> Feb 2002

REM Make all environment changes local
setlocal

REM This fixes the wierdness in condor_cpp_util.mak -stolley 08/2002
awk "{ gsub(/\.\\\.\.\\Debug/, \"..\\Debug\") } { gsub(/\.\\\.\.\\Release/, \"..\\Release\") } { print }" condor_cpp_util.mak > ~temp.mak
del condor_cpp_util.mak
ren ~temp.mak condor_cpp_util.mak

REM InputDir is stupidly defined by an absolute path by MSVS. So we
REM have to have awk clean up by making everything relative.
for %%f in ( *.mak ) do awk "{ b = gensub(/InputDir=(.*)\\src\\(.+)/, \"InputDir=..\\\\src\\\\\\2\", $0); print b }" %%f > %%f.tmp

if exist condor_util_lib.mak.tmp (
	del *.mak
	ren *.tmp *.
		) ELSE (
			echo awk makefile cleanup failed!
		   	exit /b 1
		)

REM Build the externals
call make_win32_externals.bat
if not errorlevel 0 goto failure

if defined INCLUDE goto :check_sdk
call VCVARS32.BAT
if defined INCLUDE goto :check_sdk
echo *** Visual C++ bin directory not in the path, or compiler not installed.
goto failure
:check_sdk
if defined MSSDK goto :compiler_ready
call setenv.bat /2000 /RETAIL
if defined MSSDK goto :compiler_ready
echo *** Microsoft SDK directory not in the path, or not installed.
goto failure
:compiler_ready
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug& shift & echo Debug Build - Output going to ..\Debug) else (echo Release Build - Output going to ..\Release)
call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL
REM Note - Order is important in the loop below
for %%f in ( gsoap condor_util_lib condor_cpp_util condor_io condor_classad condor_kbdd_dll condor_procapi condor_sysapi condor_daemon_core condor_acct condor_qmgmt condor_collector condor_config_val condor_dagman condor_findhost condor_history condor_mail condor_master condor_negotiator condor_preen condor_prio condor_q condor_qedit condor_submit_dag condor_rm condor_schedd condor_shadow condor_startd condor_starter condor_drmaa condor_gridmanager condor_stats condor_advertise condor_fetchlog condor_status condor_submit condor_tool condor_version condor_wait condor_userlog condor_userprio condor_store_cred condor_cod condor_transfer_data condor_birdwatcher) do ( awk "{if (($0 !~ /microsoft/) && ($0 !~ /externals/)) print }" %%f.dep > %%f.dep.tmp && del %%f.dep & ren %%f.dep.tmp %%f.dep & echo *** Building %%f & echo . & nmake /C /f %%f.mak RECURSE="0" CFG="%%f - Win32 %conf%" %* || goto failure )
echo .
echo *** Done.  Build is all happy.  Congrats!  Go drink beer.  
exit /B 0
:failure
echo .
echo *** Build Stopped.  Please fix errors and try again.
exit /B 1
