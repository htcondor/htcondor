# Microsoft Developer Studio Project File - Name="condor_cpp_util" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_cpp_util - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_cpp_util - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_cpp_util - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "condor_cpp_util - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "../src/condor_c++_util"
# PROP Intermediate_Dir "../src/condor_c++_util"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX"..\src\condor_includes\condor_common.h" /FD /TP /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../src/condor_c++_util"
# PROP Intermediate_Dir "../src/condor_c++_util"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX"..\src\condor_includes\condor_common.h" /FD /TP /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_cpp_util - Win32 Release"
# Name "condor_cpp_util - Win32 Debug"
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_config.C"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_config.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\config.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\disk.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_schedd_addr.C"
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

SOURCE="..\src\condor_c++_util\simplelist.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\up_down.C"
# End Source File
# End Target
# End Project
