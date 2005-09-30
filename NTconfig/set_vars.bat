@echo off
REM ======================================================================
REM First set all the variables that contain common bits for our build
REM environment.
REM * * * * * * * * * * * * * PLEASE READ* * * * * * * * * * * * * * * *
REM Microsoft Visual Studio will choke on variables that contain strings 
REM exceeding 255 chars, so be careful when editing this file! It's 
REM totally lame but there's nothing we can do about it.
REM ======================================================================

REM Where do the completed externals live?
if A%EXTERN_DIR%==A  set EXTERN_DIR=%cd%\..\externals
set EXT_INSTALL=%EXTERN_DIR%\install
set EXT_TRIGGERS=%EXTERN_DIR%\triggers

REM Specify which versions of the externals we're using. To add a 
REM new external, just add its version here, and add that to the 
REM EXTERNALS_NEEDED variable defined below.
set EXT_GSOAP_VERSION=gsoap-2.7
set EXT_OPENSSL_VERSION=openssl-0.9.8
set EXT_KERBEROS_VERSION=
set EXT_GLOBUS_VERSION=
set EXT_PCRE_VERSION=pcre-6.1

REM Now tell the build system what externals we need built.
set EXTERNALS_NEEDED=%EXT_GSOAP_VERSION% %EXT_OPENSSL_VERSION% %EXT_KERBEROS_VERSION% %EXT_GLOBUS_VERSION% %EXT_PCRE_VERSION%

REM Put NTConfig in the PATH, since it's got lots of stuff we need
REM like awk, gunzip, tar, bison, yacc...
set PATH=%cd%;%SystemRoot%;%SystemRoot%\system32;C:\Perl\bin;"C:\Program Files\Microsoft Visual Studio\VC98\bin";"C:\Program Files\Microsoft Platform SDK for Windows XP SP2";
call vcvars32.bat
if not defined INCLUDE ( echo . && echo *** Failed to run VCVARS32.BAT! Is Microsoft Visual Studio 6.0 installed? && exit /B 1 )
call setenv /2000 /RETAIL
if not defined MSSDK ( echo . && echo *** Failed to run SETENV.BAT! Is Microsoft Platform SDK installed? && exit /B 1 )

REM Set up some stuff for BISON
set BISON_SIMPLE=%cd%\bison.simple
set BISON_HAIRY=%cd%\bison.hairy

REM Tell the build system where we can find soapcpp2
set SOAPCPP2=%EXT_INSTALL%\%EXT_GSOAP_VERSION%\soapcpp2.exe

set CONDOR_INCLUDE=/I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /I "..\src\condor_schedd.V6" /GR
set CONDOR_LIB=Crypt32.lib mpr.lib psapi.lib mswsock.lib netapi32.lib imagehlp.lib advapi32.lib ws2_32.lib user32.lib oleaut32.lib ole32.lib
set CONDOR_LIBPATH=

REM ======================================================================
REM Now set the individual variables specific to each external package.
REM Some have been defined, but are not in use yet.
REM ======================================================================

REM ** GSOAP
set CONDOR_GSOAP_INCLUDE=/I %EXT_INSTALL%\%EXT_GSOAP_VERSION%\src
set CONDOR_GSOAP_LIB=
set CONDOR_GSOAP_LIBPATH=

REM ** GLOBUS
set CONDOR_GLOBUS_INCLUDE=
set CONDOR_GLOBUS_LIB=
set CONDOR_GLOBUS_LIBPATH=

REM ** OPENSSL
set CONDOR_OPENSSL_INCLUDE=/I %EXT_INSTALL%\%EXT_OPENSSL_VERSION%\inc32 /D CONDOR_BLOWFISH_ENCRYPTION /D CONDOR_MD /D CONDOR_ENCRYPTION /D CONDOR_3DES_ENCRYPTION
set CONDOR_OPENSSL_LIB=libeay32.lib ssleay32.lib
set CONDOR_OPENSSL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_OPENSSL_VERSION%\out32dll
rem set CONDOR_OPENSSL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_OPENSSL_VERSION%\out32dll /NODEFAULTLIB:LIBCMT.LIB

REM ** KERBEROS
REM We will uncomment this when kerberos is ready
rem set CONDOR_KERB_INCLUDE=/I %EXT_INSTALL%\%EXT_KERBEROS_VERSION%\include /D KERBEROS_AUTHENTICATION 
rem set CONDOR_KERB_LIB=comerr32.lib gssapi32.lib k5sprt32.lib krb5_32.lib xpprof32.lib
rem set CONDOR_KERB_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_KERBEROS_VERSION%\lib

REM ** PCRE
set CONDOR_PCRE_INCLUDE=/I %EXT_INSTALL%\%EXT_PCRE_VERSION%\include
set CONDOR_PCRE_LIB=libpcre.lib
set CONDOR_PCRE_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_PCRE_VERSION%\lib

