# Microsoft Developer Studio Project File - Name="condor_startd" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_startd - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_startd.mak" CFG="condor_startd - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_startd - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_startd - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_startd - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /Gm /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ../Debug/condor_common.obj ..\Debug\condor_common_c.obj pdh.lib $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /map /debug /machine:I386 /SWAPRUN:NET
# SUBTRACT LINK32 /pdb:none

!ELSEIF  "$(CFG)" == "condor_startd - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_startd___Win32_Release"
# PROP BASE Intermediate_Dir "condor_startd___Win32_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /pdb:none /debug /machine:I386 /SWAPRUN:NET
# ADD LINK32 ../Release/condor_common.obj ../Release/condor_common_c.obj pdh.lib $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /pdb:none /map /debug /machine:I386 /SWAPRUN:NET

!ENDIF 

# Begin Target

# Name "condor_startd - Win32 Debug"
# Name "condor_startd - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_startd.V6\AvailStats.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\AvailStats.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\backfill_mgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\backfill_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\boinc_mgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\boinc_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\claim.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\claim.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\cod_mgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\cod_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\command.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayCommon.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayNotifier.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\CondorSystrayNotifier.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\IdDispenser.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\IdDispenser.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\LoadQueue.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Reqexp.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Reqexp.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResAttributes.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResAttributes.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResMgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResMgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Resource.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Resource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResState.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\ResState.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\soap_startdC.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\soap_startdH.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\soap_startdServer.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\soap_startdStub.cpp
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\soap_startdStub.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronjob.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronjob.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronmgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_cronmgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\startd_main.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\StartdHookMgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\StartdHookMgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Starter.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\Starter.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\starter_mgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\starter_mgr.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\util.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\vm_common.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\vm_common.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMMachine.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMMachine.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMManager.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMManager.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMRegister.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\VMRegister.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\vmuniverse_mgr.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_startd.V6\vmuniverse_mgr.h
# End Source File
# End Target
# End Project
