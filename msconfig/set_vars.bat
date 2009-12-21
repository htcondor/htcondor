@echo off
REM ======================================================================
REM 
REM  Copyright (C) 1990-2007, Condor Team, Computer Sciences Department,
REM  University of Wisconsin-Madison, WI.
REM  
REM  Licensed under the Apache License, Version 2.0 (the "License"); you
REM  may not use this file except in compliance with the License.  You may
REM  obtain a copy of the License at
REM  
REM     http://www.apache.org/licenses/LICENSE-2.0
REM  
REM  Unless required by applicable law or agreed to in writing, software
REM  distributed under the License is distributed on an "AS IS" BASIS,
REM  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
REM  See the License for the specific language governing permissions and
REM  limitations under the License.
REM 
REM ======================================================================

REM ======================================================================
REM First set all the variables that contain common bits for our build
REM environment.
REM * * * * * * * * * * * * * PLEASE READ * * * * * * * * * * * * * * * *
REM Microsoft Visual Studio will choke on variables that contain strings 
REM exceeding 255 chars, so be careful when editing this file! It's 
REM totally lame but there's nothing we can do about it.
REM ======================================================================

REM 64-bit environments have a slightly different directory layout here
REM we take this in to account so that we can prefix paths later with the
REM the correct program files path. (Note, this assumes a clean install.
REM I'll harden it further if there is a requirement for it.)
set PROGRAMS_DIR=%ProgramFiles%
if not "A%ProgramFiles(x86)%"=="A" set PROGRAMS_DIR=%SystemDrive%\PROGRA~2

REM Set paths to Visual C++, the Platform SDKs, and Perl
set VS_DIR=%PROGRAMS_DIR%\Microsoft Visual Studio 9.0
set VC_DIR=%VS_DIR%\VC
set VC_BIN=%VC_DIR%\bin
set PERL_DIR=%SystemDrive%\Perl\bin;%SystemDrive%\Perl64\bin;%SystemDrive%\prereq\ActivePerl-5.10.1\bin
set SDK_DIR=%ProgramFiles%\Microsoft Platform SDK
set DBG_DIR=%ProgramFiles%\Debugging Tools for Windows (x86);%ProgramFiles%\Debugging Tools for Windows (x64)
set DOTNET_DIR=%SystemRoot%\Microsoft.NET\Framework\v3.5;%SystemRoot%\Microsoft.NET\Framework\v2.0.50727
set JDK_DIR="E:\Program Files\Java\jdk1.6.0_16"

REM For some reason this is not defined whilst in some environments
if "A%VS90COMNTOOLS%"=="A" set VS90COMNTOOLS=%VS_DIR%\Common7\Tools\

REM Specify symbol image path for debugging
if "A%_NT_SYMBOL_PATH%"=="A" set _NT_SYMBOL_PATH=SRV*%SystemDrive%\Symbols*http://msdl.microsoft.com/download/symbols

REM For externals: it just tells them we would like to have manifest 
REM files embeded in the rem DLLs (In the future, when we do not have 
REM VC6, we can remove this.  After we--of course--remove any reference 
REM to it from the externals)
set NEED_MANIFESTS_IN_EXTERNALS=True

REM Where do the completed externals live?
if "A%EXTERN_DIR%"=="A" set EXTERN_DIR=%cd%\..\externals
set EXT_INSTALL=%EXTERN_DIR%\install
set EXT_TRIGGERS=%EXTERN_DIR%\triggers

REM Specify which versions of the externals we're using. To add a 
REM new external, just add its version here, and add that to the 
REM EXTERNALS_NEEDED variable defined below.
set EXT_GSOAP_VERSION=gsoap-2.7.10-p5
set EXT_OPENSSL_VERSION=openssl-0.9.8h-p2
set EXT_POSTGRESQL_VERSION=postgresql-8.0.2
set EXT_KERBEROS_VERSION=krb5-1.4.3-p0
set EXT_PCRE_VERSION=pcre-7.6
set EXT_DRMAA_VERSION=drmaa-1.6
set EXT_CURL_VERSION=curl-7.19.6-p1
set EXT_HADOOP_VERSION=hadoop-0.20.0-p2

REM Now tell the build system what externals we need built.
set EXTERNALS_NEEDED=%EXT_GSOAP_VERSION% %EXT_OPENSSL_VERSION% %EXT_KERBEROS_VERSION% %EXT_PCRE_VERSION% %EXT_POSTGRESQL_VERSION% %EXT_DRMAA_VERSION% %EXT_CURL_VERSION% %EXT_HADOOP_VERSION%

REM Put msconfig in the PATH, since it's got lots of stuff we need
REM like awk, gunzip, tar, bison, yacc...
set PATH=%cd%;%SystemRoot%;%SystemRoot%\system32;%PERL_DIR%;%VS_DIR%;%VC_DIR%;%VC_BIN%;%SDK_DIR%;%DOTNET_DIR%;%DBG_DIR%;%JDK_LIB%

REM ======================================================================
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
REM Since we a still stuck in the past (i.e. supporting Win2K) we must
REM lie to the setenv script, and pretend the DevEnvDir environment
REM is alredy configured properly (yay! jump to VC2K8, but support
REM Win2K... *sigh*) 
set MSVCDir=%VC_DIR%
set DevEnvDir=%VS_DIR%\Common7\IDE
set MSVCVer=9.0
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
REM ======================================================================

REM Configure Visual C++
call "%VC_DIR%\vcvarsall.bat" x86
if not defined INCLUDE ( echo. && echo *** Failed to run vcvarsall.bat! Is Microsoft Visual Studio installed? && exit /B 1 )

REM ======================================================================
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
REM We set these here as the above script will find the wrong versions
REM of everything... *sigh*
set WindowsSdkDir=%SDK_DIR%
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
REM ======================================================================

REM Configure the Platform SDK environment
call "%SDK_DIR%\SetEnv.Cmd" /2000 /RETAIL
if not defined MSSDK ( echo. && echo *** Failed to run setenv.cmd! Are the Microsoft Platform SDK installed? && exit /B 1 )

REM ======================================================================
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
set INCLUDE=%SDK_DIR%\Include;%VC_DIR%\ATLMFC\INCLUDE;%VC_DIR%\INCLUDE;
set LIB=%SDK_DIR%\Lib;%VC_DIR%\ATLMFC\LIB;%VC_DIR%\LIB;
set LIBPATH=%DOTNET_DIR%;%VC_DIR%\ATLMFC\LIB;%VC_DIR%\LIB;
REM ====== THIS SHOULD BE REMOVED WHEN Win2K IS NO LONGER SUPPORTED ======
REM ======================================================================

REM Set up some stuff for BISON
set BISON_SIMPLE=%cd%\bison.simple
set BISON_HAIRY=%cd%\bison.hairy

REM Tell the build system where we can find soapcpp2
set SOAPCPP2=%EXT_INSTALL%\%EXT_GSOAP_VERSION%\soapcpp2.exe

REM Determine the build id, if it is defined
pushd ..
set BID=none
if exist BUILD-ID. (
    echo Found BUILD-ID in %cd%
    for /f %%i in ('more BUILD-ID') do set BID=%%i
) else (
    echo No build-id defined: %cd%\BUILD-ID is missing.
)
echo Using build-id: %BID% & echo.
popd

REM Determine the number of processor we can run concurrent jobs on.  We base
REM this number on the count of cores or CPUs kept by the OS:
if "A%NUMBER_OF_PROCESSORS%"=="A" set PROCESSORS=1
set PROCESSORS=%NUMBER_OF_PROCESSORS%

set CONDOR_NOWARN=/D_CRT_SECURE_NO_DEPRECATE /D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_WARNINGS /D_CRT_NON_CONFORMING_SWPRINTFS
REM /D_CONST_RETURN
set CONDOR_CPPARGS=/GR /MP%PROCESSORS%
set CONDOR_DEFINE=/DHAVE_CONFIG_H /DBUILDID=%BID% %CONDOR_CPPARGS% %CONDOR_NOWARN%
set CONDOR_INCLUDE=/I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /I "..\src\condor_schedd.V6" /I "..\src\classad" /I "..\src\ccb" /I "..\src\condor_io"
set CONDOR_LIB=crypt32.lib mpr.lib psapi.lib mswsock.lib netapi32.lib imagehlp.lib ws2_32.lib powrprof.lib iphlpapi.lib userenv.lib
set CONDOR_LIBPATH=/NOLOGO

REM ======================================================================
REM Now set the individual variables specific to each external package.
REM Some have been defined, but are not in use yet (they have /NOLOGO
REM to make them a no-op during the build).
REM ======================================================================

REM ** GSOAP
set CONDOR_GSOAP_INCLUDE=/I %EXT_INSTALL%\%EXT_GSOAP_VERSION%\src
set CONDOR_GSOAP_LIB=/NOLOGO
set CONDOR_GSOAP_LIBPATH=/NOLOGO

REM ** OPENSSL
set CONDOR_OPENSSL_INCLUDE=/I %EXT_INSTALL%\%EXT_OPENSSL_VERSION%\inc32
set CONDOR_OPENSSL_LIB=libeay32.lib ssleay32.lib
set CONDOR_OPENSSL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_OPENSSL_VERSION%\out32dll

REM ** POSTGRESQL
set CONDOR_POSTGRESQL_INCLUDE=/I %EXT_INSTALL%\%EXT_POSTGRESQL_VERSION%\inc32
set CONDOR_POSTGRESQL_LIB=libpqdll.lib
set CONDOR_POSTGRESQL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_POSTGRESQL_VERSION%\out32dll

REM ** KERBEROS
set CONDOR_KERB_INCLUDE=/I %EXT_INSTALL%\%EXT_KERBEROS_VERSION%\include
set CONDOR_KERB_LIB=comerr32.lib gssapi32.lib k5sprt32.lib krb5_32.lib xpprof32.lib
set CONDOR_KERB_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_KERBEROS_VERSION%\lib

REM ** PCRE
set CONDOR_PCRE_INCLUDE=/I %EXT_INSTALL%\%EXT_PCRE_VERSION%\include
set CONDOR_PCRE_LIB=libpcre.lib
set CONDOR_PCRE_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_PCRE_VERSION%\lib

REM ** JDK
set CONDOR_JDK_INCLUDE=/I %JDK_DIR%\include /I %JDK_DIR%\include\win32
set CONDOR_JDK_LIB=jvm.lib
set CONDOR_JDK_LIBPATH=/LIBPATH:%JDK_DIR%\lib

REM Dump the Windows build environment at this point
REM echo ----------------------- WIN ENV DUMP ----------------------
REM set
REM echo ----------------------- WIN ENV DUMP ----------------------

exit /B 0
