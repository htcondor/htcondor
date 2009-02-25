
REM . 
REM Manages the building of the PCRE external on Win32
REM . 

REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%
set PACKAGE_BUILD_DIR=%PACKAGE_BUILD_DIR:/=\%
set PACKAGE_SRC_DIR=%PACKAGE_BUILD_DIR%\pcre-7.6

REM Assume success...
set RET_CODE=0
pushd .
cd %PACKAGE_BUILD_DIR%
cd
copy Makefile.win32 %PACKAGE_SRC_DIR%
copy config.h.win32 %PACKAGE_SRC_DIR%\config.h
copy pcre.h.win32 %PACKAGE_SRC_DIR%\pcre.h
copy libpcre.def.win32 %PACKAGE_SRC_DIR%\libpcre.def
touch zerolength
gmake -f Makefile.win32 -C %PACKAGE_SRC_DIR% < zerolength
"mkdir" -p %PACKAGE_INSTALL_DIR%\lib
"mkdir" -p %PACKAGE_INSTALL_DIR%\include
copy %PACKAGE_SRC_DIR%\libpcre.lib %PACKAGE_INSTALL_DIR%\lib || set RET_CODE=1
copy %PACKAGE_SRC_DIR%\libpcre.dll %PACKAGE_INSTALL_DIR%\lib || set RET_CODE=1
copy %PACKAGE_SRC_DIR%\pcre.h %PACKAGE_INSTALL_DIR%\include || set RET_CODE=1

exit %RET_CODE%
