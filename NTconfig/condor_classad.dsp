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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
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
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /Ob2 /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
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

SOURCE=..\src\classad.old\ast.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\astbase.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\attrlist.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\buildtable.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\classad.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\classad_list.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\classad_util.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\classifiedjobs.cpp
# SUBTRACT CPP /YX /Yc /Yu
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

SOURCE=..\src\condor_includes\classad.old.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\classad.old_lookup.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\classad.old_util.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_classifiedjobs.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_commands.h
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

SOURCE=..\src\condor_includes\condor_registration.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_scanner.h
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\environment.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\evaluateOperators.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\new_classads.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\operators.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\operators.h
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\parser.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\registration.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\scanner.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\value.cpp
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\value.h
# End Source File
# Begin Source File

SOURCE=..\src\classad.old\xml_classads.cpp
# End Source File
# End Target
# End Project
