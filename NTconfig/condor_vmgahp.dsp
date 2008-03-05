# Microsoft Developer Studio Project File - Name="condor_vmgahp" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Console Application" 0x0103

CFG=condor_vmgahp - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_vmgahp.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_vmgahp.mak" CFG="condor_vmgahp - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_vmgahp - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_vmgahp - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_vmgahp - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# ADD LINK32 ../Debug/condor_common.obj ..\Debug\condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /debug /machine:I386 /out:"..\Debug/condor_vm-gahp.exe" /pdbtype:sept
# SUBTRACT LINK32 /map

!ELSEIF  "$(CFG)" == "condor_vmgahp - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_vmgahp___Win32_Release"
# PROP BASE Intermediate_Dir "condor_vmgahp___Win32_Release"
# PROP BASE Ignore_Export_Lib 0
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "../Release"
# PROP Intermediate_Dir "../Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:console /debug /machine:I386 /pdbtype:sept
# SUBTRACT BASE LINK32 /map
# ADD LINK32 ../Release/condor_common.obj ..\Release\condor_common_c.obj $(CONDOR_LIB) $(CONDOR_LIBPATH) $(CONDOR_GSOAP_LIB) $(CONDOR_GSOAP_LIBPATH) $(CONDOR_KERB_LIB) $(CONDOR_KERB_LIBPATH) $(CONDOR_GLOBUS_LIBPATH) $(CONDOR_GLOBUS_LIB) $(CONDOR_PCRE_LIB) $(CONDOR_PCRE_LIBPATH) $(CONDOR_OPENSSL_LIB) $(CONDOR_POSTGRESQL_LIB) $(CONDOR_OPENSSL_LIBPATH) $(CONDOR_POSTGRESQL_LIBPATH) /nologo /subsystem:console /pdb:none /map /debug /machine:I386 /out:"../Release/condor_vm-gahp.exe"

!ENDIF 

# Begin Target

# Name "condor_vmgahp - Win32 Debug"
# Name "condor_vmgahp - Win32 Release"
# Begin Source File

SOURCE="..\src\condor_vm-gahp\gsoap_vmgahp.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\pbuffer.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\pbuffer.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\soap_vmgahpC.C"

!IF  "$(CFG)" == "condor_vm-gahp - Win32 Debug"

!ELSEIF  "$(CFG)" == "condor_vm-gahp - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\soap_vmgahpH.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\soap_vmgahpServer.C"

!IF  "$(CFG)" == "condor_vm-gahp - Win32 Debug"

!ELSEIF  "$(CFG)" == "condor_vm-gahp - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\soap_vmgahpStub.C"

!IF  "$(CFG)" == "condor_vm-gahp - Win32 Debug"

!ELSEIF  "$(CFG)" == "condor_vm-gahp - Win32 Release"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\soap_vmgahpStub.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vm_request.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vm_request.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vm_type.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vm_type.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_common.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_common.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_config.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_config.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_error_codes.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_instantiate.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmgahp_main.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmware_type.C"
# End Source File
# Begin Source File

SOURCE="..\src\condor_vm-gahp\vmware_type.h"
# End Source File
# End Target
# End Project
