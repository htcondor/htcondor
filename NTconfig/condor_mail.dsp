# Microsoft Developer Studio Project File - Name="condor_mail" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_mail - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_mail.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_mail.mak" CFG="condor_mail - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_mail - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_mail - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_mail - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /FD /TP /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ws2_32.lib mswsock.lib netapi32.lib /nologo /subsystem:console /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /profile /debug

!ELSEIF  "$(CFG)" == "condor_mail - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_mail___Win32_Release"
# PROP BASE Intermediate_Dir "condor_mail___Win32_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 1
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /Gf /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /FD /TP /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib ws2_32.lib mswsock.lib netapi32.lib ../src/condor_c++_util/condor_common.obj ..\src\condor_util_lib/condor_common.obj /nologo /subsystem:console /machine:I386 /pdbtype:sept
# SUBTRACT BASE LINK32 /profile /debug
# ADD LINK32 ws2_32.lib mswsock.lib netapi32.lib /nologo /subsystem:console /pdb:none /map /debug /machine:I386
# SUBTRACT LINK32 /profile

!ENDIF 

# Begin Target

# Name "condor_mail - Win32 Debug"
# Name "condor_mail - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_mail\condor_email_main.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_mail\resource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_mail\smtp.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_mail\smtp.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_mail\StdAfx.h
# End Source File
# End Target
# End Project
