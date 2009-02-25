
REM .
REM Manages the building of the drmaa external on Win32
REM .

REM FYI: Here's the variables we should have available:
REM PACKAGE_NAME
REM PACKAGE_BUILD_DIR
REM PACKAGE_INSTALL_DIR
REM EXTERNALS_INSTALL_DIR

REM Some magic to make sure PACKAGE_INSTALL_DIR has no forward slashes in it
set PACKAGE_INSTALL_DIR=%PACKAGE_INSTALL_DIR:/=\%

REM Assume success...
set RET_CODE=0

md %PACKAGE_INSTALL_DIR%
md %PACKAGE_INSTALL_DIR%\src
md %PACKAGE_INSTALL_DIR%\include
md %PACKAGE_INSTALL_DIR%\lib

REM copy the source tree to install location
xcopy /S /Y * %PACKAGE_INSTALL_DIR%\src || set RET_CODE=1

REM build the drmaa library
nmake /f libdrmaa.mak CFG="libdrmaa - Win32 Release"
copy Release\libdrmaa.lib %PACKAGE_INSTALL_DIR%\lib || set RET_CODE=1
copy Release\libdrmaa.dll %PACKAGE_INSTALL_DIR%\lib || set RET_CODE=1
copy drmaa.h %PACKAGE_INSTALL_DIR%\include || set RET_CODE=1

exit %RET_CODE%

