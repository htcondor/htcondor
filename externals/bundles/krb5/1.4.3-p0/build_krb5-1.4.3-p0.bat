
REM . 
REM Manages the building of the gsoap external on Win32
REM . 

REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%

REM temporary debug output to try and diagnose an error that appears in the nightly
REM builds but not in a workspace build of a fresh checkout -gq
set
echo %CD%

REM adjust the MAXHOSTNAMELEN to the value in condor
cp win-mac.pre.h krb5-1.4.3-p0\src\include\win-mac.h
echo %ERRORLEVEL%

REM get into the source / build dir
pushd krb5-1.4.3-p0\src

REM munge the makefile for windows
nmake -f Makefile.in prep-windows
REM Err prep-windows is %ERRORLEVEL%
IF ERRORLEVEL 1 exit 1


REM do the actual build, which will fail when building gss-sample
REM but we've got the libs we need at that point
nmake
REM Err nmake is %ERRORLEVEL%

REM go back to starting dir
popd

REM now patch up the header file to get rid of things condor
REM already defines
cp win-mac.post.h krb5-1.4.3-p0\src\include\win-mac.h
echo %ERRORLEVEL%

REM now "install" the libs and include files
mkdir %PACKAGE_INSTALL_DIR%\include
mkdir %PACKAGE_INSTALL_DIR%\lib
cp krb5-1.4.3-p0\src\lib\obj\i386\rel\* %PACKAGE_INSTALL_DIR%\lib
cp krb5-1.4.3-p0\src\include\* %PACKAGE_INSTALL_DIR%\include

REM hacky as hell, but we decide we were successful if all the libs we
REM need are there.  anything missing and we exit 1.

cd %PACKAGE_INSTALL_DIR%\lib

IF NOT EXIST comerr32.lib exit 1
IF NOT EXIST gssapi32.lib exit 1
IF NOT EXIST k5sprt32.lib exit 1
IF NOT EXIST krb5_32.lib exit 1
IF NOT EXIST xpprof32.lib exit 1

rem If we are using VC8, then we need to embed a manifest into the DLLs we created.
rem This allows them to locate the new CRT libs.
if /i A%NEED_MANIFESTS_IN_EXTERNALS%==Atrue ( 
    rem Embed manifest into DLLs
    for %%f in (*.dll) do mt.exe /manifest %%f.manifest /outputresource:%%f;#2
    IF ERRORLEVEL 1 exit 1
)

REM good to go...

exit 0

