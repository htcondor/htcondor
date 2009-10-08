
REM . 
REM Manages the building of the openssl external on Win32
REM . 

REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_SRC_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%
set PACKAGE_BUILD_DIR=%PACKAGE_BUILD_DIR:/=\%

REM Now patch the build system to get the Multi-threaded runtime library
REM For now, this is disabled, since I don't think we need it to make
REM condor happy. -stolley

cd %PACKAGE_SRC_NAME%
rem touch zerolength
rem patch -p1 -i ..\openssl-0.9.8-patch < zerolength
rem if not %ERRORLEVEL%A == 0A goto failure

rem Patch the Makefile builder:
rem The current version of openssl user strcpy, etc. when it should use
rem strncpy (or so say the MS security experts). Anyway, instead of changing
rem openssl code, we simply disable the Visual Studio check by defining:
rem _CRT_SECURE_NO_DEPRECATE at the command line. The VC-32 bla, bla, bla,
rem part removes all the deprecated compiler options openssl passes to VC. 
rem We do this because all warnings are treated as errors in this build. 
if /i A%NEED_MANIFESTS_IN_EXTERNALS%==Atrue ( 
    pushd util
    touch zerolength
    call :patch %PACKAGE_BUILD_DIR%\mk1mf.pl-0.9.8h-patch
    if not %ERRORLEVEL%A == 0A goto failure
    pushd pl
    touch zerolength
    call :patch %PACKAGE_BUILD_DIR%\VC-32.pl-0.9.8h-patch
    if not %ERRORLEVEL%A == 0A goto failure
    popd && popd
)

perl Configure VC-WIN32
if not %ERRORLEVEL%A == 0A goto failure

call ms\do_nt.bat
if not %ERRORLEVEL%A == 0A goto failure

nmake -f ms\ntdll.mak
if not %ERRORLEVEL%A == 0A goto failure

REM ** copy output into install dir
mkdir %PACKAGE_INSTALL_DIR%\out32dll
mkdir %PACKAGE_INSTALL_DIR%\inc32

xcopy out32dll\*.lib %PACKAGE_INSTALL_DIR%\out32dll
if not %ERRORLEVEL%A == 0A goto failure
xcopy out32dll\*.dll %PACKAGE_INSTALL_DIR%\out32dll
if not %ERRORLEVEL%A == 0A goto failure

REM If we are using a modern version of VC, then we need to embed a manifest 
REM into the DLLs we created. This allows them to locate the new CRT libs.
if /i A%NEED_MANIFESTS_IN_EXTERNALS%==Atrue (
    pushd %PACKAGE_INSTALL_DIR%\out32dll
    for %%f in (*.dll) do mt.exe /manifest %PACKAGE_BUILD_DIR%\%%f.manifest /outputresource:%%f;#2
    if not %ERRORLEVEL%A == 0A goto failure
    popd
)

xcopy /s inc32\*.* %PACKAGE_INSTALL_DIR%\inc32
if not %ERRORLEVEL%A == 0A goto failure

:failure
exit %ERRORLEVEL%

rem ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
:patch
rem ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
rem function: patch <file_path>
rem  purpose: calls patch.exe in the context of the debugger if patch
rem           fails.  This seems to resolve the errorlevel 3 problem.  
rem           Hurray for mysteriously, magical, nonsensical solutions!
rem ::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
patch --verbose -i %1 < zerolength
if ERRORLEVEL 3 ( 
  set ERRORLEVEL=0
  cdb -o -c "g; !gle; kb; q" patch --verbose -i %1 < zerolength
)
goto :EOF
