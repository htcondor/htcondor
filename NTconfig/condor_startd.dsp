# Microsoft Developer Studio Project File - Name="condor_startd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_startd - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak" CFG="condor_startd - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_startd - Win32 Debug" (based on\
 "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_startd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\src\condor_startd.V6"
# PROP Intermediate_Dir "..\src\condor_startd.V6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib condor_c++_util\condor_common.obj /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\src\condor_startd.V6"
# PROP Intermediate_Dir "..\src\condor_startd.V6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib pdh.lib ws2_32.lib ../src/condor_c++_util/condor_common.obj ../src/condor_util_lib/condor_common.obj /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT LINK32 /incremental:no /map

!ENDIF 

# Begin Target

# Name "condor_startd - Win32 Release"
# Name "condor_startd - Win32 Debug"
# Begin Source File

SOURCE=..\src\condor_startd.V6\calc.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\calc.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\main.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Match.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Match.h
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

SOURCE=..\src\condor_startd.V6\Starter.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Starter.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.h
# End Source File
# End Target
# End Project
