# Microsoft Developer Studio Project File - Name="condor_qmgmt" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_qmgmt - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_qmgmt.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_qmgmt.mak" CFG="condor_qmgmt - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_qmgmt - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_qmgmt - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_qmgmt - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_qmgmt - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_qmgmt___Win32_Release"
# PROP BASE Intermediate_Dir "condor_qmgmt___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP /c
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

# Name "condor_qmgmt - Win32 Debug"
# Name "condor_qmgmt - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_includes\condor_qmgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgmt_common.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgmt_constants.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgmt_send_stubs.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgr_job_updater.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgr_job_updater.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_schedd.V6\qmgr_lib_support.C
# End Source File
# End Target
# End Project
