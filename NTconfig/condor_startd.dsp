# Microsoft Developer Studio Project File - Name="condor_startd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_startd - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak" CFG="condor_startd - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_startd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ../Debug/condor_common.obj ..\Debug\condor_common_c.obj Crypt32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib mswsock.lib netapi32.lib imagehlp.lib mpr.lib /nologo /subsystem:console /map /debug /machine:I386 /SWAPRUN:NET
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_startd___Win32_Release"
# PROP BASE Intermediate_Dir "condor_startd___Win32_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 ../src/condor_c++_util/condor_common.obj ../src/condor_util_lib/condor_common.obj kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib mswsock.lib netapi32.lib /nologo /subsystem:console /pdb:none /debug /machine:I386 /SWAPRUN:NET
# ADD LINK32 ../Release/condor_common.obj ../Release/condor_common_c.obj Crypt32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib mswsock.lib netapi32.lib imagehlp.lib mpr.lib /nologo /subsystem:console /pdb:none /map /debug /machine:I386 /SWAPRUN:NET

!ENDIF 

# Begin Target

# Name "condor_startd - Win32 Debug"
# Name "condor_startd - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_startd.V6\AvailStats.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\AvailStats.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\claim.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\claim.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\cod_mgr.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\cod_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayCommon.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayNotifier.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayNotifier.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\mds.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\mds.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Reqexp.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Reqexp.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResAttributes.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResAttributes.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResMgr.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResMgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Resource.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Resource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResState.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResState.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronjob.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronjob.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronmgr.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronmgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_main.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Starter.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Starter.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\starter_mgr.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\starter_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.h
# End Source File
# End Target
# End Project
