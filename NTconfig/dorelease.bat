@echo off
if exist %1 goto process1
echo Error - directory %1 does not exist
echo Usage: release.bat path_to_release_subdirectory
goto end
:process1
if exist ..\Release goto process2
echo Compile a Release Build of Condor first.
goto end
:process2
echo Copying files...
copy ..\Release\*.exe %1
copy ..\Release\*.dll %1
copy ..\src\condor_starter.V6.1\*.class %1
copy msvcrt.dll %1
copy msvcirt.dll %1
copy pdh.dll %1
echo Stripping out debug symbols from files...
pushd %1
for %%f in (*.exe) do rebase -b 0x400000 -x . -a %%f
for %%f in (master startd schedd collector negotiator shadow starter eventd) do move condor_%%f.dbg condor_%%f.save
del *.dbg 
for %%f in (master startd schedd collector negotiator shadow starter eventd) do move condor_%%f.save condor_%%f.dbg
copy condor_rm.exe condor_hold.exe
copy condor_rm.exe condor_release.exe
copy condor_rm.exe condor_vacate_job.exe
copy condor.exe condor_off.exe
copy condor.exe condor_on.exe
copy condor.exe condor_restart.exe
copy condor.exe condor_reconfig.exe
copy condor.exe condor_reschedule.exe
copy condor.exe condor_vacate.exe
popd
:end
