@echo off
REM Build Condor from a batch file
REM Todd Tannenbaum <tannenba@cs.wisc.edu> Feb 2002
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug& shift & echo Debug Build - Output going to ..\Debug) else (echo Release Build - Output going to ..\Release)
call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL
REM Note - Order is important in the loop below
for %%f in ( condor_util_lib condor_cpp_util condor_io condor_classad condor_kbdd_dll condor_procapi condor_sysapi condor_daemon_core condor_acct condor_qmgmt condor_collector condor_config_val condor_dagman condor_findhost condor_history condor_mail condor_master condor_negotiator condor_preen condor_prio condor_q condor_qedit condor_rm condor_schedd condor_shadow condor_startd condor_starter condor_stats condor_status condor_submit condor_tool condor_userlog condor_userprio) do ( echo *** Building %%f & echo . & nmake /C /f %%f.mak RECURSE="0" CFG="%%f - Win32 %conf%" %* || goto failure )
echo .
echo *** Done.  Build is all happy.  Congrats!
exit /B 0
:failure
echo .
echo *** Build Stopped.  Please fix errors and try again.
exit /B 1
