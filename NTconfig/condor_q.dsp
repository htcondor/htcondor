# Microsoft Developer Studio Project File - Name="condor_q" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_q - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_q.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_q.mak" CFG="condor_q - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_q - Win32 Release" (based on\
 "Win32 (x86) Console Application")
!MESSAGE "condor_q - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_q - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../src/condor_q.V6"
# PROP Intermediate_Dir "../src/condor_q.V6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /TP /c
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /machine:I386
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /machine:I386

!ELSEIF  "$(CFG)" == "condor_q - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../src/condor_q.V6"
# PROP Intermediate_Dir "../src/condor_q.V6"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /TP /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib wsock32.lib /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "condor_q - Win32 Release"
# Name "condor_q - Win32 Debug"
# Begin Source File

SOURCE=..\src\condor_includes\_condor_fix_nt.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\_condor_fix_resource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\_condor_fix_types.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_ast.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_astbase.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_attributes.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_attrlist.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_classad.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_common.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_config.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_constants.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_expressions.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_exprtype.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_mach_status.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_network.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.h"
# End Source File
# Begin Source File

SOURCE=..\src\h\files.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\list.h"
# End Source File
# Begin Source File

SOURCE=..\src\h\proc.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\query_result_type.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_q.V6\queue.C
# End Source File
# Begin Source File

SOURCE=..\src\h\sched.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\simplelist.h"
# End Source File
# Begin Source File

SOURCE=..\src\h\startup.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\stream.h
# End Source File
# End Target
# End Project
