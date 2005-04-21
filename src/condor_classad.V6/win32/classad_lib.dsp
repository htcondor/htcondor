# Microsoft Developer Studio Project File - Name="classad_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=classad_lib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "classad_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "classad_lib.mak" CFG="classad_lib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "classad_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "classad_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "classad_lib - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "_DEBUG" /D "WIN32" /D "_WINDOWS" /D "CLASSAD_DISTRIBUTION" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c
# SUBTRACT CPP /YX /Yc /Yu
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "classad_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "classad_lib___Win32_Release"
# PROP BASE Intermediate_Dir "classad_lib___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /Ob2 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "NDEBUG" /D "WIN32" /D "_WINDOWS" /D "CLASSAD_DISTRIBUTION" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c
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

# Name "classad_lib - Win32 Debug"
# Name "classad_lib - Win32 Release"
# Begin Source File

SOURCE=..\attrrefs.C
# End Source File
# Begin Source File

SOURCE=..\attrrefs.h
# End Source File
# Begin Source File

SOURCE=..\classad.C
# End Source File
# Begin Source File

SOURCE=..\classad.h
# End Source File
# Begin Source File

SOURCE=..\classad_distribution.h
# End Source File
# Begin Source File

SOURCE=..\classad_stl.h
# End Source File
# Begin Source File

SOURCE=..\classadErrno.h
# End Source File
# Begin Source File

SOURCE=..\classadItor.h
# End Source File
# Begin Source File

SOURCE=..\collection.C
# End Source File
# Begin Source File

SOURCE=..\collection.h
# End Source File
# Begin Source File

SOURCE=..\collectionBase.C
# End Source File
# Begin Source File

SOURCE=..\collectionBase.h
# End Source File
# Begin Source File

SOURCE=..\common.h
# End Source File
# Begin Source File

SOURCE=..\debug.C
# End Source File
# Begin Source File

SOURCE=..\debug.h
# End Source File
# Begin Source File

SOURCE=..\exprList.C
# End Source File
# Begin Source File

SOURCE=..\exprList.h
# End Source File
# Begin Source File

SOURCE=..\exprTree.C
# End Source File
# Begin Source File

SOURCE=..\exprTree.h
# End Source File
# Begin Source File

SOURCE=..\fnCall.C
# End Source File
# Begin Source File

SOURCE=..\fnCall.h
# End Source File
# Begin Source File

SOURCE=..\indexfile.C
# End Source File
# Begin Source File

SOURCE=..\indexfile.h
# End Source File
# Begin Source File

SOURCE=..\lexer.C
# End Source File
# Begin Source File

SOURCE=..\lexer.h
# End Source File
# Begin Source File

SOURCE=..\lexerSource.C
# End Source File
# Begin Source File

SOURCE=..\lexerSource.h
# End Source File
# Begin Source File

SOURCE=..\literals.C
# End Source File
# Begin Source File

SOURCE=..\literals.h
# End Source File
# Begin Source File

SOURCE=..\matchClassad.C
# End Source File
# Begin Source File

SOURCE=..\matchClassad.h
# End Source File
# Begin Source File

SOURCE=..\operators.C
# End Source File
# Begin Source File

SOURCE=..\operators.h
# End Source File
# Begin Source File

SOURCE=..\query.C
# End Source File
# Begin Source File

SOURCE=..\query.h
# End Source File
# Begin Source File

SOURCE=..\sink.C
# End Source File
# Begin Source File

SOURCE=..\sink.h
# End Source File
# Begin Source File

SOURCE=..\source.C
# End Source File
# Begin Source File

SOURCE=..\source.h
# End Source File
# Begin Source File

SOURCE=..\transaction.C
# End Source File
# Begin Source File

SOURCE=..\transaction.h
# End Source File
# Begin Source File

SOURCE=..\util.C
# End Source File
# Begin Source File

SOURCE=..\util.h
# End Source File
# Begin Source File

SOURCE=..\value.C
# End Source File
# Begin Source File

SOURCE=..\value.h
# End Source File
# Begin Source File

SOURCE=..\view.C
# End Source File
# Begin Source File

SOURCE=..\view.h
# End Source File
# Begin Source File

SOURCE=..\xmlLexer.C
# End Source File
# Begin Source File

SOURCE=..\xmlLexer.h
# End Source File
# Begin Source File

SOURCE=..\xmlSink.C
# End Source File
# Begin Source File

SOURCE=..\xmlSink.h
# End Source File
# Begin Source File

SOURCE=..\xmlSource.C
# End Source File
# Begin Source File

SOURCE=..\xmlSource.h
# End Source File
# End Target
# End Project
