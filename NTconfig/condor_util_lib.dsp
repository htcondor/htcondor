# Microsoft Developer Studio Project File - Name="condor_util_lib" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_util_lib - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_util_lib.mak" CFG="condor_util_lib - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_util_lib - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_util_lib - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common_c.pch" /Yu"condor_common.h" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_util_lib___Win32_Release"
# PROP BASE Intermediate_Dir "condor_util_lib___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_util_lib/condor_common.pch" /Yu"condor_common.h" /FD /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common_c.pch" /Yu"condor_common.h" /FD /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"
# ADD LIB32 /nologo /out:"..\src\condor_util_lib\condor_util.lib"

!ENDIF 

# Begin Target

# Name "condor_util_lib - Win32 Debug"
# Name "condor_util_lib - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_util_lib\basename.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\blankline.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\chomp.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\ckpt_name.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_common_c.C
# ADD CPP /Yc"condor_common.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_email.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_perms.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_perms.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_snutils.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_uid.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\condor_universe.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\copy_file.c
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

SOURCE=..\src\condor_util_lib\exit_utils.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\fdprintf.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\fdprintf.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\filename_tools.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\get_exec_path.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\get_port_range.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\get_random_num.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\globus_utils.c
# End Source File
# Begin Source File

SOURCE=..\src\h\globus_utils.h
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

SOURCE=..\src\condor_util_lib\nullfile.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\printf_format.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\proc_id.c
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
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\h\syscall_numbers.tmpl

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\condor\workspaces\v67\src\h
InputPath=..\src\h\syscall_numbers.tmpl

"..\src\h\syscall_numbers.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(InputDir)\..\..\NTconfig\awk.exe -f $(InputDir)\awk_prog.include_file $(InputDir)\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

# End Custom Build

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

# PROP BASE Ignore_Default_Tool 1
# PROP Ignore_Default_Tool 1
# Begin Custom Build
InputDir=\condor\workspaces\v67\src\h
InputPath=..\src\h\syscall_numbers.tmpl

"..\src\h\syscall_numbers.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	$(InputDir)\..\..\NTconfig\awk.exe -f $(InputDir)\awk_prog.include_file $(InputDir)\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

# End Custom Build

!ENDIF 

# End Source File
# Begin Source File

SOURCE=..\src\condor_util_lib\win32_posix.c
# End Source File
# End Target
# End Project
