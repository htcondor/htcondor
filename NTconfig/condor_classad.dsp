# Microsoft Developer Studio Project File - Name="condor_classad" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_classad - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak" CFG="condor_classad - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_classad - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_classad - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "condor_classad - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\src\condor_classad"
# PROP Intermediate_Dir "..\src\condor_classad"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /TP /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\src\condor_classad"
# PROP Intermediate_Dir "..\src\condor_classad"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FR /YX /FD /TP /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_classad - Win32 Release"
# Name "condor_classad - Win32 Debug"
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

SOURCE=..\src\condor_classad\ast.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\astbase.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\attrlist.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\buildtable.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\classad.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\classad_util.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\classifiedjobs.C
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

SOURCE=..\src\condor_includes\condor_buildtable.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_classad.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_classad_util.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_classifiedjobs.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_common.h
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

SOURCE=..\src\condor_includes\condor_registration.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_scanner.h
# End Source File
# Begin Source File

SOURCE=..\src\h\debug.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\environment.C
# End Source File
# Begin Source File

SOURCE=..\src\h\except.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\parser.C
# End Source File
# Begin Source File

SOURCE=..\src\h\proc.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\registration.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad\scanner.C
# End Source File
# Begin Source File

SOURCE=..\src\h\sched.h
# End Source File
# Begin Source File

SOURCE=..\src\h\startup.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\stream.h
# End Source File
# End Target
# End Project
