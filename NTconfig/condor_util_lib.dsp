# Microsoft Developer Studio Project File - Name="condor_util_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_util_lib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_util_lib - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_util_lib - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\src\condor_util_lib"
# PROP Intermediate_Dir "..\src\condor_util_lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\src\condor_util_lib"
# PROP Intermediate_Dir "..\src\condor_util_lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ENDIF 

# Begin Target

# Name "condor_util_lib - Win32 Release"
# Name "condor_util_lib - Win32 Debug"
# Begin Source File

SOURCE=..\src\condor_includes\_condor_fix_nt.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\_condor_fix_types.h
# End Source File
# Begin Source File

SOURCE=..\src\h\clib.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_common.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_debug.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_errlst.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_expressions.h
# End Source File
# Begin Source File

SOURCE=..\src\h\condor_sys.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_uid.h
# End Source File
# Begin Source File

SOURCE=..\src\h\debug.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\dprintf.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\except.c
# End Source File
# Begin Source File

SOURCE=..\src\h\except.h
# End Source File
# Begin Source File

SOURCE=..\src\h\exit.h
# End Source File
# Begin Source File

SOURCE=..\src\h\expr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\internet.c
# End Source File
# Begin Source File

SOURCE=..\src\h\internet.h
# End Source File
# Begin Source File

SOURCE=..\src\h\proc.h
# End Source File
# Begin Source File

SOURCE=..\src\h\syscall_numbers.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\uids.c
# End Source File
# End Target
# End Project
