# Microsoft Developer Studio Project File - Name="condor_ckpt_server_api" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 5.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_ckpt_server_api - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_ckpt_server_api.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_ckpt_server_api.mak"\
 CFG="condor_ckpt_server_api - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_ckpt_server_api - Win32 Release" (based on\
 "Win32 (x86) Static Library")
!MESSAGE "condor_ckpt_server_api - Win32 Debug" (based on\
 "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe

!IF  "$(CFG)" == "condor_ckpt_server_api - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\src\condor_ckpt_server"
# PROP Intermediate_Dir "..\src\condor_ckpt_server"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h" /FD /c
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_ckpt_server_api - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\src\condor_ckpt_server"
# PROP Intermediate_Dir "..\src\condor_ckpt_server"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h" /FD /c
# SUBTRACT CPP /Fr
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_ckpt_server_api - Win32 Release"
# Name "condor_ckpt_server_api - Win32 Debug"
# Begin Source File

SOURCE=..\src\condor_ckpt_server\constants2.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_ckpt_server\network2.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_ckpt_server\network2.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_ckpt_server\server_interface.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_ckpt_server\server_interface.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_ckpt_server\typedefs2.h
# End Source File
# End Target
# End Project
