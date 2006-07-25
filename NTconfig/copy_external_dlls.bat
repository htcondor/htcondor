@echo off

REM Script to copy all .DLL files found in the built externals into
REM the Debug and Release subdirs so they get shipped with Condor.
REM Must be run from within NTConfig, and EXT_INSTALL must be set
REM (which is usually done by set_vars.bat).
REM Returns 0 on success, 1 on failure.

REM Assume success...
set RET_CODE=0

REM Make certain we start out in the NTconfig subdir
if %0==%~n0 goto good
if %0==%~n0.bat goto good
echo You must be in the NTconfig subdirectory
set RET_CODE=1
goto finish

REM Now copy all .dll files found anywhere in EXT_INSTALL into Debug
REM and Release.
:good
if A%EXT_INSTALL%==A ( echo Env var EXT_INSTALL not specified & set RET_CODE=1 & goto finish )
set DEBUG_DIR=%cd%\..\Debug
set RELEASE_DIR=%cd%\..\Release
if not exist %DEBUG_DIR%\NUL mkdir %DEBUG_DIR%
if not exist %RELEASE_DIR%\NUL mkdir %RELEASE_DIR%
for /R %EXTERN_DIR% %%w in (*.dll) do (copy /Y %%w %DEBUG_DIR% || set RET_CODE=1) > NUL: 2>1
for /R %EXTERN_DIR% %%w in (*.dll) do (copy /Y %%w %RELEASE_DIR% || set RET_CODE=1) > NUL: 2>1


REM Since we changed the cwd above, switch back and exit.
:finish
echo copy_external_dlls.bat return code is %RET_CODE%
exit /b %RET_CODE%
