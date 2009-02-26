
REM . 
REM Manages the building of the postgres external on Win32
REM Note: On Win32, we only worry about building the postgres client
REM . 

REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_SRC_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%

REM Set the current working dir
cd %PACKAGE_SRC_NAME%

REM Apply patch file, if it exists.
rem touch zerolength
rem patch -p1 -i ..\postgresql-8.2.3-patch < zerolength
rem if not %ERRORLEVEL%A == 0A goto failure

REM Build the client libs
cd src\interfaces\libpq
nmake -f win32.mak
if not %ERRORLEVEL%A == 0A goto failure

REM ** copy output into install dir
mkdir %PACKAGE_INSTALL_DIR%\out32dll
mkdir %PACKAGE_INSTALL_DIR%\inc32

xcopy Release\*.lib %PACKAGE_INSTALL_DIR%\out32dll
if not %ERRORLEVEL%A == 0A goto failure
xcopy Release\*.dll %PACKAGE_INSTALL_DIR%\out32dll
if not %ERRORLEVEL%A == 0A goto failure

xcopy libpq-fe.h %PACKAGE_INSTALL_DIR%\inc32
if not %ERRORLEVEL%A == 0A goto failure

cd ..\..\include

xcopy /s *.* %PACKAGE_INSTALL_DIR%\inc32
if not %ERRORLEVEL%A == 0A goto failure

:failure
exit %ERRORLEVEL%

