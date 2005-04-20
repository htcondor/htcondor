# Microsoft Developer Studio Project File - Name="condor_io" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_io - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak" CFG="condor_io - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_io - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_io - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_io - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_io___Win32_Release"
# PROP BASE Intermediate_Dir "condor_io___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) /c
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

# Name "condor_io - Win32 Debug"
# Name "condor_io - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_io\authentication.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\authentication.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\buffers.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\buffers.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\cedar_no_ckpt.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_anonymous.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_claim.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_claim.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_sspi.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_sspi.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_crypt.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_crypt.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_io.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_rw.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_rw.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_secman.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\CryptKey.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\CryptKey.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\errno_num.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\open_flags.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\reli_sock.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\reli_sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\safe_sock.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\safe_sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\SafeMsg.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\SafeMsg.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\sock.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\sockCache.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\sockCache.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\stream.C
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\stream.h
# End Source File
# End Target
# End Project
