# Microsoft Developer Studio Project File - Name="condor_classad" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_classad - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_classad.mak" CFG="condor_classad - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_classad - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_classad - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_classad - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /FD /TP /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_classad - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_classad___Win32_Release"
# PROP BASE Intermediate_Dir "condor_classad___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /Ob2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD CPP /nologo /MD /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "CLASSAD_DISTRIBUTION" /FD /TP /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_classad - Win32 Debug"
# Name "condor_classad - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_classad.V6\attrrefs.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\attrrefs.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classad.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classad.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classad_distribution.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classad_stl.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classadErrno.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\classadItor.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\collection.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\collection.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\collectionBase.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\collectionBase.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\common.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\debug.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\debug.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\exprList.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\exprList.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\exprTree.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\exprTree.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\fnCall.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\fnCall.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\indexfile.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\indexfile.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\lexer.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\lexer.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\lexerSource.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\lexerSource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\literals.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\literals.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\matchClassad.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\matchClassad.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\operators.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\operators.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\query.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\query.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\sink.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\sink.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\source.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\source.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\transaction.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\transaction.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\util.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\util.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\value.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\value.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\view.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\view.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlLexer.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlLexer.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlSink.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlSink.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlSource.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_classad.V6\xmlSource.h
# End Source File
# End Target
# End Project
