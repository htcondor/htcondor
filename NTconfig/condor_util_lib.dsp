# Microsoft Developer Studio Project File - Name="condor_util_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_util_lib - Win32 Debug
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak" CFG="condor_util_lib - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_util_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_util_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 0
# PROP BASE Output_Dir "Release"
# PROP BASE Intermediate_Dir "Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 0
# PROP Output_Dir "..\src\condor_util_lib"
# PROP Intermediate_Dir "..\src\condor_util_lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MT /W3 /GX /O2 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\src\condor_util_lib"
# PROP Intermediate_Dir "..\src\condor_util_lib"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MDd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h" /FD /c
# SUBTRACT CPP /Fr
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ENDIF 

# Begin Target

# Name "condor_util_lib - Win32 Release"
# Name "condor_util_lib - Win32 Debug"
# Begin Source File

SOURCE=..\src\condor_util_lib\basename.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\blankline.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\ckpt_name.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_common.C
# ADD CPP /Yc"condor_common.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_email.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_uid.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\cronos.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\d_format_time.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\dprintf.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\dprintf_common.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\dprintf_config.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\email.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\email_file.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\escapes.c
# ADD CPP /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\except.c
# End Source File
# Begin Source File

SOURCE=..\src\h\except.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\fdprintf.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\fdprintf.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\get_random_num.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\internet.c
# End Source File
# Begin Source File

SOURCE=..\src\h\internet.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\lock_file.WIN32.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\ltrunc.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\match_prefix.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\mkargv.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\rotate_file.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\setsyscalls.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\signames.c
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\strcmp_until.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

# SUBTRACT CPP /YX /Yc /Yu

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\h\syscall_numbers.tmpl

!IF  "$(CFG)" == "condor_util_lib - Win32 Release"

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputPath=..\src\h\syscall_numbers.tmpl

"..\src\h\syscall_numbers.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(InputDir)\..\..\NTconfig\awk.exe -f $(InputDir)\awk_prog.include_file $(InputDir)\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

# End Custom Build

!ENDIF 

# End Source File
# End Target
# End Project
