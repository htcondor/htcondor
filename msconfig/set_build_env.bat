@echo off
REM --------------------------------------------------------------
REM Set PATH, INCLUDE, and LIB environment variables so
REM that CMAKE can be invoked and Condor can be built from
REM the command line.
REM
REM Note that the existing PATH is not preserved, although the new
REM path will still have Windows, PERL, and CMAKE in it.
REM
REM if USER_TOOLS_DIR and/or USER_TOOLS_DIR2 are set, they will be
REM added to the path, USER_TOOLS_DIR will be a the beginning
REM and USER_TOOLS_DIR2 will be at the end of the path.
REM
REM This batch file will setup the following environment variables
REM if they do not already exist.
REM  CMAKE_BIN_DIR - short path to cmake\bin
REM  WIX_BIN_DIR   - short path to Wix\bin
REM  ZIP_BIN_DIR   - short path to 7-Zip
REM  PERL_BIN_DIR  - short path to Perl (c:\Perl\bin)
REM  GIT_BIN_DIR   - short path to git\bin
REM  VS_DIR        - the short path to the Visual Studio 2008 directory tree
REM  VC_DIR        - the short path to %VS_DIR%\VC (used to set include and lib)
REM  VC_BIN        - the short path to %VS_DIR%\VC\bin (where the compiler is)
REM  SDK_DIR       - the short path to the Windows SDK (v7.0)
REM  DOTNET_DIRS   - paths to .NET 3.5 and 2.0
REM
REM  PATH, LIB, and INCLUDE are then constructed from these
REM
REM Requirements:
REM
REM   before executing this batch script, CMAKE.EXE must
REM   be in the PATH, or CMAKE_BIN_DIR must be set to its location
REM   or CMAKE must be installed in c:\tools\cmake
REM
REM   MSCONFIG_TOOLS_DIR must be set to the condor sources msconfig dir
REM   or this batch file must be run from that directory
REM
REM   the location of the MS C compiler is derived from the
REM   VS90COMNTOOLS environment variable. (which is set in all
REM   user's environments by the default compiler install.)
REM
REM

REM assume that this file is in the msconfig subfolder of the condor
REM sources or that MSCONFIG_TOOLS_DIR is set.
if "~%MSCONFIG_TOOLS_DIR%"=="~" set MSCONFIG_TOOLS_DIR=%~sdp0
if "~%MSCONFIG_TOOLS_DIR:~-1%"=="~\" Set MSCONFIG_TOOLS_DIR=%MSCONFIG_TOOLS_DIR:~0,-1%
if not exist %MSCONFIG_TOOLS_DIR%\license.rtf echo MSCONFIG_TOOLS_DIR is not valid!

REM cmake should be in the path, or CMAKE_BIN_DIR must be set,
REM we don't want CMAKE_BIN_DIR to have a trailing slash.
if "~%CMAKE_BIN_DIR%"=="~" for %%I in (cmake.exe) DO Set CMAKE_BIN_DIR=%%~sdp$PATH:I
if "~%CMAKE_BIN_DIR%"=="~" set CMAKE_BIN_DIR=c:\Tools\CMake\bin
if "~%CMAKE_BIN_DIR:~-1%"=="~\" Set CMAKE_BIN_DIR=%CMAKE_BIN_DIR:~0,-1%
echo CMAKE_BIN_DIR=%CMAKE_BIN_DIR%
if not exist %CMAKE_BIN_DIR%\cmake.exe echo CMake is not in CMAKE_BIN_DIR!

REM WiX should be in the path or WIX_BIN_DIR must be set
if "~%WIX_BIN_DIR%"=="~" for %%I in (wix.dll) DO Set WIX_BIN_DIR=%%~sdp$PATH:I
if "~%WIX_BIN_DIR%"=="~" set WIX_BIN_DIR=c:\Tools\WiX\bin
if "~%WIX_BIN_DIR:~-1%"=="~\" Set WIX_BIN_DIR=%WIX_BIN_DIR:~0,-1%
echo WIX_BIN_DIR=%WIX_BIN_DIR%
if not exist %WIX_BIN_DIR%\wix.dll echo Wix is not in WIX_BIN_DIR!

REM need the 7zip path also
if "~%ZIP_BIN_DIR%"=="~" for %%I in (7z.exe) do set ZIP_BIN_DIR=%%~sdp$PATH:I
if "~%ZIP_BIN_DIR%"=="~" set ZIP_BIN_DIR=c:\Tools\7-zip
if "~%ZIP_BIN_DIR:~-1%"=="~\" Set ZIP_BIN_DIR=%ZIP_BIN_DIR:~0,-1%
echo ZIP_BIN_DIR=%ZIP_BIN_DIR%
if not exist %ZIP_BIN_DIR%\7z.exe echo 7z.exe is not in ZIP_BIN_DIR!

REM setup GIT_BIN_DIR if we can find git
REM
if "~%GIT_BIN_DIR%"=="~" for %%I in (git.exe) Do Set GIT_BIN_DIR=%%~sdp$PATH:I
if "~%GIT_BIN_DIR%"=="~" set GIT_BIN_DIR=c:\Tools\Git\bin
if "~%GIT_BIN_DIR:~-1%"=="~\" Set GIT_BIN_DIR=%GIT_BIN_DIR:~0,-1%
echo GIT_BIN_DIR=%GIT_BIN_DIR%
if not exist %GIT_BIN_DIR%\git.exe echo Git.exe is not in GIT_BIN_DIR!

REM set USER_TOOLS_DIR to point to c:\tools if it exists and doesn't
REM already point to something.
if "~%USER_TOOLS_DIR%"=="~" (
  if exist c:\Tools\NUL set USER_TOOLS_DIR=C:\Tools
)
echo USER_TOOLS_DIR=%USER_TOOLS_DIR%


REM 64-bit environments have a slightly different directory layout here
REM we take this in to account so that we can prefix paths later with the
REM the correct program files path. (Note, this assumes a clean install.
REM I'll harden it further if there is a requirement for it.)
set PROGRAMS_DIR=%ProgramFiles%
if not "~%ProgramFiles(x86)%"=="~" set PROGRAMS_DIR=%SystemDrive%\PROGRA~2


if "~%VS90COMNTOOLS%"=="~" goto use_default_paths
REM don't guess about paths, derive them from available from VS90COMNTOOLS 
REM
for /D %%I in ("%VS90COMNTOOLS%..") do set VS90ROOT=%%~sdpI
set VS_DIR=%VS90ROOT:~0,-1%
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VC_DIR%\bin
set DBG_TOOLS_DIR=%ProgramFiles%\Debugging Tools for Windows (x86);%ProgramFiles%\Debugging Tools for Windows (x64)
set DOTNET_DIRS=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727

if "~%SDK_DIR%"=="~"  set SDK_DIR=%SystemDrive%\PROGRA~1\MICROS~3\Windows\v7.0
rem if "~%SDK_DIR%"=="~"  set SDK_DIR=%SystemDrive%\Win2003Sdk
if "~%PERL_BIN_DIR%"=="~" set PERL_BIN_DIR=%SystemDrive%\Perl\bin
goto got_paths


REM don't have VS90COMNTOOLS defined, so we will have to assume default
REM installation paths
REM
:use_default_paths

REM Set paths to Visual C++, the Platform SDKs, and Perl
set VS_DIR=%PROGRAMS_DIR%\Microsoft Visual Studio 9.0
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VC_DIR%\bin
set PERL_BIN_DIR=%SystemDrive%\Perl\bin
set SDK_DIR=%ProgramFiles%\Microsoft SDKs\Windows\v7.0
set DBG_TOOLS_DIR=%ProgramFiles%\Debugging Tools for Windows (x86);%ProgramFiles%\Debugging Tools for Windows (x64)
set DOTNET_DIRS=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727


:got_paths


REM Setup the path to point to Windows, the C compiler, msconfig for stuff
REM like awk, gunzip, tar, bison, yacc...
REM
set PATH=%SystemRoot%;%SystemRoot%\system32;%CMAKE_BIN_DIR%;%MSCONFIG_TOOLS_DIR%
set PATH=%PATH%;%VS_DIR%\Common7\IDE;%VC_DIR%;%VC_BIN%
if NOT "~%SDK_DIR%"=="~" set PATH=%PATH%;%SDK_DIR%\bin
if NOT "~%PERL_BIN_DIR%"=="~" set PATH=%PATH%;%PERL_BIN_DIR%
if NOT "~%DOTNET_DIRS%"=="~" set PATH=%PATH%;%DOTNET_DIRS%
REM if NOT "~%DBG_TOOLS_DIR%"=="~" set PATH=%PATH%;%DBG_TOOLS_DIR%
if NOT "~%WIX_BIN_DIR%"=="~" set PATH=%PATH%;%WIX_BIN_DIR%
if NOT "~%GIT_BIN_DIR%"=="~" set PATH=%PATH%;%GIT_BIN_DIR%
if NOT "~%ZIP_BIN_DIR%"=="~" set PATH=%PATH%;%ZIP_BIN_DIR%
if NOT "~%USER_TOOLS_DIR%"=="~" set PATH=%USER_TOOLS_DIR%;%PATH%
if NOT "~%USER_TOOLS_DIR2%"=="~" set PATH=%PATH%;%USER_TOOLS_DIR2%

REM setup include path
REM
set INCLUDE=%VC_DIR%\include
set INCLUDE=%INCLUDE%;%VC_DIR%\atlmfc\include
if NOT "~%SDK_DIR%"=="~" set INCLUDE=%INCLUDE%;%SDK_DIR%\include
if NOT "~%DOTNET_DIRS%"=="~" set INCLUDE=%INCLUDE%;%DOTNET_DIRS%

REM setup lib path
REM
set LIB=%VC_DIR%\lib
set LIB=%LIB%;%VC_DIR%\atlmfc\lib
if NOT "~%SDK_DIR%"=="~" set LIB=%LIB%;%SDK_DIR%\lib

REM Dump the Windows build environment at this point
REM echo ----------------------- WIN ENV DUMP ----------------------
@echo PATH=%PATH%
@echo INCLUDE=%INCLUDE%
@echo LIB=%LIB%
REM echo ----------------------- WIN ENV DUMP ----------------------

exit /B 0
