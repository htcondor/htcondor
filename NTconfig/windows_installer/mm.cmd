@echo off
set MAKEMSI_DB_COMPARE_TEMPLATE.MSI=%cd%\MakeMSI\UiSample.MSI
set PPWIZARD_TEST_REGINA_VER=3.3(JULIAN)(MT)
set MAKEMSI_DIR=%cd%\MakeMSI\
set MAKEMSI_MSIVAL2_DIR=
set MAKEMSI_WILOGUTL_EXE=
set REGINA_MACROS=%cd%\MakeMSI
set PPWIZARD_INCLUDE=%cd%\MakeMsi\
setlocal
if A%CONDORRELEASEDIR%==A set CONDORRELEASEDIR=%cd%\..\..\release-dir
set PATH=%PATH%;"%cd%\MakeMSI";%SystemRoot%\system32

@REM *** PVCS: $Header: /space/home/matt/CVS2GIT/CONDOR_SRC-src/NTconfig/windows_installer/mm.cmd,v 1.2 2006-07-25 18:15:29 wright Exp $
@REM ************************************
@REM ******** RC = 0 means OK ***********
@REM ************************************


@REM *** Initialization *****************************************************
setlocal
cls
md out > nul 2>&1


@REM *** Get parameters (if .MM not named see if ONLY 1) ********************
set MmName=%~n1
set MmMode=%2
@REM ***
@REM *** MAKEMSI_MM_PRIORITY
@REM *** ~~~~~~~~~~~~~~~~~~~
@REM *** This environment variable can be used to run MAKEMSI faster or
@REM *** slower than normal. This should contain the text of any priority
@REM *** switch which is valid on the Windows "START" command, for example
@REM *** "BELOWNORMAL" or the less likely "LOW". You would lower the priority
@REM *** where you know MAKEMSI will take a while and you still want your
@REM *** computer to respond quickly to your "other" tasks.
@REM ***
@REM *** MAKEMSI_MM
@REM *** ~~~~~~~~~~
@REM *** The contents of this environment variable is added to the end of
@REM *** the PPWIZARD command line so can override most of the previously
@REM *** mentioned switches as well as allowing you to add others.
@REM *** If you need to modify this batch file then tell me why!
@REM ***
@REM *** MAKEMSI_USER_FILES_PATH
@REM *** ~~~~~~~~~~~~~~~~~~~~~~~
@REM *** The contents of these environment variables are added to the
@REM *** list of directories PPWIZARD/MAKEMSI will search for files (header etc).
@REM *** See the PPWIZARD "/IncludePath" doco for more information.
@REM ***


@REM *** Change Buffer size, unless told not too ****************************
set BuffSize=%MAKEMSI_MM_CONBUFFSIZE%
if     "%BuffSize%" == ""   set BuffSize=32000
if not "%BuffSize%" == "-"  ConSetBuffer.exe /H=%BuffSize%


@Rem *** What modes are supported ([D]evelopment and [P]roduction by default) ***
set MmModeList=%MAKEMSI_MMMODES%
if "%MmModeList%" == "" set MmModeList=DdPp


@REM *** If no parameters at all then start looking! ************************
if "%MmName%" == ""  goto LookForMm0p


@REM *** Was MM name omitted so only mode passed? ***************************
set MmModeListT=%MmModeList%
:TryNext
   set FC=%MmModeListT:~0,1%
   set MmModeListT=%MmModeListT:~1%
   if "%MmName%" == "%FC%" goto LookForMm1p
   if not "%MmModeListT%" == "" goto TryNext
goto HaveMmName
:LookForMm1p
   set MmMode=%MmName%
:LookForMm0p
   echo @echo off >                  out\MsiTmp.cmd
   echo set NumFiles=%%NumFiles%%#>> out\MsiTmp.cmd
   echo set   MmName=%%~n1>>         out\MsiTmp.cmd
   set MmName=
   set NumFiles=
   for %%f in (*.mm) do call out\MsiTmp.cmd %%f
   @rem echo T.MmName   = "%MmName%"
   @rem echo T.MmMode   = "%MmMode%"
   @rem echo T.NumFiles = "%NumFiles%"
   if not "%NumFiles%" == "#" goto ERR_NOPARM
:HaveMmName
del out\MsiTmp.cmd >nul 2>&1
if "%MmName%" == "" goto ERR_NOPARM
if not exist "%MmName%.mm" goto ERR_WHERE


REM *** The first mode is the default ***************************************
if  "%MMMODE%" == "" set MMMODE=%MmModeList:~0,1%


REM *** Work Out where the output LOG/MSI etc goes **************************
set RootDir=
echo set RootDir=%%MAKEMSI_MM_ROOTDIR.%MMMODE%%%> out\MsiTmp.cmd
call out\MsiTmp.cmd
del out\MsiTmp.cmd >nul 2>&1
if  "%RootDir%" == "" set RootDir=out\*.*


@REM *** May be useful for diagnosis ****************************************
del out\%MmName%.CON.TXT >nul 2>&1
SET DEBUGFILE=out\%MmName%.DBG.TXT
echo [%date% %time% - Diagnostic info cmd]: VER   > "%DEBUGFILE%" 2>&1
VER                              >> "%DEBUGFILE%" 2>&1
echo.                            >> "%DEBUGFILE%" 2>&1
echo.                            >> "%DEBUGFILE%" 2>&1
echo [%date% %time% - Diagnostic info cmd]: SET  >> "%DEBUGFILE%" 2>&1
SET                              >> "%DEBUGFILE%" 2>&1
echo.                            >> "%DEBUGFILE%" 2>&1
echo.                            >> "%DEBUGFILE%" 2>&1


REM *** Work out the command line we will use *******************************
SET MakeCmd=reg4mm.exe PPWIZ4MM.4MM '/ConsoleFile:+%DEBUGFILE%' /ErrorFile: /OTHER '%MmName%.mm'
set MakeCmd=%MakeCmd% /Define:MmMode=%MmMode%
set MakeCmd=%MakeCmd% /DeleteOnError:N
set MakeCmd=%MakeCmd% '/Output:%RootDir%\IgnoreMe.txt'
set MakeCmd=%MakeCmd% /Sleep:0,0 '/BuildTitle:/OTHER/Building from {x22}{IS}{x22}'
set MakeCmd=%MakeCmd% /option:Tabs='4'
set MakeCmd=%MakeCmd% /IncludePath=*MAKEMSI_USER_FILES_PATH;*MAKEMSI_COMPANY_SUPPORT_DIR;
set MakeCmd=%MakeCmd% %MAKEMSI_MM%
if     "%MAKEMSI_MM_BEEPS%"    == "N" set MakeCmd=%MakeCmd% /Beep:N
if not "%MAKEMSI_MM_PRIORITY%" == ""  set MakeCmd=START /B /WAIT /%MAKEMSI_MM_PRIORITY% %MakeCmd%

REM *** Change Codepage (log to console Log) ********************************
if  "%MAKEMSI_MM_CODEPAGE%" == ""  set MAKEMSI_MM_CODEPAGE=1252
if  "%MAKEMSI_MM_CODEPAGE%" == "?" goto NoChCp
    set CodePageCmd=chcp %MAKEMSI_MM_CODEPAGE%
    echo [%date% %time% - CMD]: %CodePageCmd% >> "%DEBUGFILE%" 2>&1
    %CodePageCmd%     >> "%DEBUGFILE%" 2>&1
    echo.             >> "%DEBUGFILE%" 2>&1
:NoChCp

REM *** Output the PPWIZARD command line to the console log *****************
echo [%date% %time% - CMD]: %MakeCmd% >> "%DEBUGFILE%" 2>&1
echo.                 >> "%DEBUGFILE%" 2>&1

REM *** "REGINA_MACROS" must include MAKEMSI install dir (set by install, but lets make sure) ***
SET REGINA_MACROS=%MAKEMSI_DIR%;%REGINA_MACROS%

REM *** Now Run the job *****************************************************
%MakeCmd%
if errorlevel 1 goto ENDBATCH_ERROR
goto ENDBATCH_OK



REM *** Error Handling ******************************************************
:ERR_NOPARM
   @echo.
   @echo ERROR: NO PARAMETER SUPPLIED
   goto SHOW_WI
:ERR_WHERE
   @echo.
   @echo ERROR: Can't find "%MmName%.mm"
   goto SHOW_WI
:SHOW_WI
   @echo.
   @echo AVAILABLE SCRIPTS
   @echo ~~~~~~~~~~~~~~~~~
   dir *.mm /on /b
   set DEBUGFILE=nul
   goto ENDBATCH_ERROR_BEEP
:ENDBATCH_ERROR_BEEP
   if not "%MAKEMSI_MM_BEEPS%" == "N" @echo 
:ENDBATCH_ERROR
   if not "%MAKEMSI_MM_BEEPS%" == "N" @echo 
   set SetRcCmd=cmd.exe /c ThisCmdWillFailGeneratingANonZeroReturnCode!.ABC
   set MmStatus=FAILED
   goto ENDBATCH


Rem *** EXIT ****************************************************************
:ENDBATCH_OK
   @echo.
   set SetRcCmd=cmd.exe /c echo This command always works so return code 0 results
   set MmStatus=OK
:ENDBATCH


@REM *** May be useful for diagnosis ****************************************
echo.                                     >> "%DEBUGFILE%" 2>&1
echo.                                     >> "%DEBUGFILE%" 2>&1
echo [%date% %time% - Diagnostic info STATUS]: %MmStatus% >> "%DEBUGFILE%" 2>&1
%SetRcCmd% >nul 2>&1
