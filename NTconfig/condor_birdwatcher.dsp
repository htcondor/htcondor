# Microsoft Developer Studio Project File - Name="condor_birdwatcher" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Application" 0x0101

CFG=condor_birdwatcher - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_birdwatcher.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_birdwatcher.mak" CFG="condor_birdwatcher - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_birdwatcher - Win32 Release" (based on "Win32 (x86) Application")
!MESSAGE "condor_birdwatcher - Win32 Debug" (based on "Win32 (x86) Application")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
MTL=midl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_birdwatcher - Win32 Release"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "..\Release"
# PROP BASE Intermediate_Dir "..\Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MT /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Yu"stdafx.h" /FD /c
# ADD CPP /nologo /MT /W3 /GX /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /c
# ADD BASE MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "NDEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "NDEBUG"
# ADD RSC /l 0x409 /d "NDEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /machine:I386
# ADD LINK32 /nologo /subsystem:windows /incremental:yes /machine:I386
# SUBTRACT LINK32 /profile /map /debug

!ELSEIF  "$(CFG)" == "condor_birdwatcher - Win32 Debug"

# PROP BASE Use_MFC 5
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "..\Debug"
# PROP BASE Intermediate_Dir "..\Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 5
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Ignore_Export_Lib 0
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Yu"stdafx.h" /FD /GZ /c
# ADD CPP /nologo /MTd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /D "_MBCS" /Yu"stdafx.h" /FD /GZ /c
# ADD BASE MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD MTL /nologo /D "_DEBUG" /mktyplib203 /win32
# ADD BASE RSC /l 0x409 /d "_DEBUG"
# ADD RSC /l 0x409 /d "_DEBUG"
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LINK32=link.exe
# ADD BASE LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept
# ADD LINK32 /nologo /subsystem:windows /debug /machine:I386 /pdbtype:sept

!ENDIF 

# Begin Target

# Name "condor_birdwatcher - Win32 Release"
# Name "condor_birdwatcher - Win32 Debug"
# Begin Group "Source Files"

# PROP Default_Filter "cpp;c;cxx;rc;def;r;odl;idl;hpj;bat"
# Begin Source File

SOURCE=..\src\condor_birdwatcher\birdwatcher.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\birdwatcher.rc
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\birdWatcherDlg.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\StdAfx.cpp
# ADD CPP /Yc"stdafx.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\SystrayManager.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\SystrayMinimize.cpp
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\WindowsMessageReceiver.cpp
# End Source File
# End Group
# Begin Group "Header Files"

# PROP Default_Filter "h;hpp;hxx;hm;inl"
# Begin Source File

SOURCE=..\src\condor_birdwatcher\birdwatcher.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\birdWatcherDlg.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\Resource.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\StdAfx.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\SystrayManager.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\SystrayMinimize.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\WindowsMessageReceiver.h
# End Source File
# End Group
# Begin Group "Resource Files"

# PROP Default_Filter "ico;cur;bmp;dlg;rc2;rct;bin;rgs;gif;jpg;jpeg;jpe"
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\birdwatcher.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\birdwatcher.rc2
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\condor_f.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\condor_s.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\ico00001.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\ico00002.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\ico00003.ico
# End Source File
# Begin Source File

SOURCE=..\src\condor_birdwatcher\res\icon1.ico
# End Source File
# End Group
# End Target
# End Project
