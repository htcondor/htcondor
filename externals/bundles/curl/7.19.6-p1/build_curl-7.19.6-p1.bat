
REM . 
REM Manages the building of the curl external on Win32
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
set PACKAGE_SRC_DIR=%PACKAGE_BUILD_DIR%\curl-7.19.6

REM Assume success...
set RET_CODE=0

cd %PACKAGE_SRC_DIR%\lib

REM curl come with different makefiles for vc6, vc8, and vc9
REM Probably should detect which vc we are using and use the right makefile
REM biggest difference is vc9 uses the bufferoverlow checking options in
REM vc9, which is good to have on.

REM nmake -f Makefile.vc9 CFG=release 
nmake -f Makefile.vc9 CFG=release-ssl OPENSSL_PATH=%EXTERNALS_INSTALL_DIR%\%EXT_OPENSSL_VERSION%

if not %ERRORLEVEL%A == 0A goto failure

"mkdir" -p %PACKAGE_INSTALL_DIR%\lib
"mkdir" -p %PACKAGE_INSTALL_DIR%\include\curl

copy %PACKAGE_SRC_DIR%\lib\lib*.lib %PACKAGE_INSTALL_DIR%\lib || set RET_CODE=1
copy %PACKAGE_SRC_DIR%\include\curl\*.h %PACKAGE_INSTALL_DIR%\include\curl || set RET_CODE=1

exit %RET_CODE%

:failure
exit %ERRORLEVEL%
