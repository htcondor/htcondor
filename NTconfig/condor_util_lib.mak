# Microsoft Developer Studio Generated NMAKE File, Based on condor_util_lib.dsp
!IF "$(CFG)" == ""
CFG=condor_util_lib - Win32 Release
!MESSAGE No configuration specified. Defaulting to condor_util_lib - Win32 Release.
!ENDIF 

!IF "$(CFG)" != "condor_util_lib - Win32 Debug" && "$(CFG)" != "condor_util_lib - Win32 Release"
!MESSAGE Invalid configuration "$(CFG)" specified.
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
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

OUTDIR=.\..\Debug
INTDIR=.\..\Debug

ALL : "..\src\h\syscall_numbers.h" "..\src\condor_util_lib\condor_util.lib"


CLEAN :
	-@erase "$(INTDIR)\basename.obj"
	-@erase "$(INTDIR)\blankline.obj"
	-@erase "$(INTDIR)\chomp.obj"
	-@erase "$(INTDIR)\ckpt_name.obj"
	-@erase "$(INTDIR)\condor_common_c.obj"
	-@erase "$(INTDIR)\condor_common_c.pch"
	-@erase "$(INTDIR)\condor_perms.obj"
	-@erase "$(INTDIR)\condor_snutils.obj"
	-@erase "$(INTDIR)\condor_universe.obj"
	-@erase "$(INTDIR)\copy_file.obj"
	-@erase "$(INTDIR)\cronos.obj"
	-@erase "$(INTDIR)\d_format_time.obj"
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf_common.obj"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\email_file.obj"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\fdprintf.obj"
	-@erase "$(INTDIR)\filename_tools.obj"
	-@erase "$(INTDIR)\get_exec_path.obj"
	-@erase "$(INTDIR)\get_port_range.obj"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\globus_utils.obj"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\nullfile.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\rotate_file.obj"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\signames.obj"
	-@erase "$(INTDIR)\strcmp_until.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\win32_posix.obj"
	-@erase "..\src\condor_util_lib\condor_util.lib"
	-@erase "..\src\h\syscall_numbers.h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_util_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"..\src\condor_util_lib\condor_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\basename.obj" \
	"$(INTDIR)\blankline.obj" \
	"$(INTDIR)\chomp.obj" \
	"$(INTDIR)\ckpt_name.obj" \
	"$(INTDIR)\condor_common_c.obj" \
	"$(INTDIR)\condor_perms.obj" \
	"$(INTDIR)\condor_snutils.obj" \
	"$(INTDIR)\condor_universe.obj" \
	"$(INTDIR)\copy_file.obj" \
	"$(INTDIR)\cronos.obj" \
	"$(INTDIR)\d_format_time.obj" \
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_common.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\email_file.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\fdprintf.obj" \
	"$(INTDIR)\filename_tools.obj" \
	"$(INTDIR)\get_port_range.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\globus_utils.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\nullfile.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\rotate_file.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
	"$(INTDIR)\strcmp_until.obj" \
	"$(INTDIR)\win32_posix.obj" \
	"$(INTDIR)\get_exec_path.obj"

"..\src\condor_util_lib\condor_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

OUTDIR=.\..\Release
INTDIR=.\..\Release

ALL : "..\src\h\syscall_numbers.h" "..\src\condor_util_lib\condor_util.lib"


CLEAN :
	-@erase "$(INTDIR)\basename.obj"
	-@erase "$(INTDIR)\blankline.obj"
	-@erase "$(INTDIR)\chomp.obj"
	-@erase "$(INTDIR)\ckpt_name.obj"
	-@erase "$(INTDIR)\condor_common_c.obj"
	-@erase "$(INTDIR)\condor_common_c.pch"
	-@erase "$(INTDIR)\condor_perms.obj"
	-@erase "$(INTDIR)\condor_snutils.obj"
	-@erase "$(INTDIR)\condor_universe.obj"
	-@erase "$(INTDIR)\copy_file.obj"
	-@erase "$(INTDIR)\cronos.obj"
	-@erase "$(INTDIR)\d_format_time.obj"
	-@erase "$(INTDIR)\dprintf.obj"
	-@erase "$(INTDIR)\dprintf_common.obj"
	-@erase "$(INTDIR)\dprintf_config.obj"
	-@erase "$(INTDIR)\email.obj"
	-@erase "$(INTDIR)\email_file.obj"
	-@erase "$(INTDIR)\escapes.obj"
	-@erase "$(INTDIR)\except.obj"
	-@erase "$(INTDIR)\fdprintf.obj"
	-@erase "$(INTDIR)\filename_tools.obj"
	-@erase "$(INTDIR)\get_exec_path.obj"
	-@erase "$(INTDIR)\get_port_range.obj"
	-@erase "$(INTDIR)\get_random_num.obj"
	-@erase "$(INTDIR)\globus_utils.obj"
	-@erase "$(INTDIR)\internet.obj"
	-@erase "$(INTDIR)\lock_file.WIN32.obj"
	-@erase "$(INTDIR)\ltrunc.obj"
	-@erase "$(INTDIR)\match_prefix.obj"
	-@erase "$(INTDIR)\mkargv.obj"
	-@erase "$(INTDIR)\nullfile.obj"
	-@erase "$(INTDIR)\proc_id.obj"
	-@erase "$(INTDIR)\rotate_file.obj"
	-@erase "$(INTDIR)\setsyscalls.obj"
	-@erase "$(INTDIR)\signames.obj"
	-@erase "$(INTDIR)\strcmp_until.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\win32_posix.obj"
	-@erase "..\src\condor_util_lib\condor_util.lib"
	-@erase "..\src\h\syscall_numbers.h"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_util_lib.bsc" 
BSC32_SBRS= \
	
LIB32=link.exe -lib
LIB32_FLAGS=/nologo /out:"..\src\condor_util_lib\condor_util.lib" 
LIB32_OBJS= \
	"$(INTDIR)\basename.obj" \
	"$(INTDIR)\blankline.obj" \
	"$(INTDIR)\chomp.obj" \
	"$(INTDIR)\ckpt_name.obj" \
	"$(INTDIR)\condor_common_c.obj" \
	"$(INTDIR)\condor_perms.obj" \
	"$(INTDIR)\condor_snutils.obj" \
	"$(INTDIR)\condor_universe.obj" \
	"$(INTDIR)\copy_file.obj" \
	"$(INTDIR)\cronos.obj" \
	"$(INTDIR)\d_format_time.obj" \
	"$(INTDIR)\dprintf.obj" \
	"$(INTDIR)\dprintf_common.obj" \
	"$(INTDIR)\dprintf_config.obj" \
	"$(INTDIR)\email.obj" \
	"$(INTDIR)\email_file.obj" \
	"$(INTDIR)\escapes.obj" \
	"$(INTDIR)\except.obj" \
	"$(INTDIR)\fdprintf.obj" \
	"$(INTDIR)\filename_tools.obj" \
	"$(INTDIR)\get_port_range.obj" \
	"$(INTDIR)\get_random_num.obj" \
	"$(INTDIR)\globus_utils.obj" \
	"$(INTDIR)\internet.obj" \
	"$(INTDIR)\lock_file.WIN32.obj" \
	"$(INTDIR)\ltrunc.obj" \
	"$(INTDIR)\match_prefix.obj" \
	"$(INTDIR)\mkargv.obj" \
	"$(INTDIR)\nullfile.obj" \
	"$(INTDIR)\proc_id.obj" \
	"$(INTDIR)\rotate_file.obj" \
	"$(INTDIR)\setsyscalls.obj" \
	"$(INTDIR)\signames.obj" \
	"$(INTDIR)\strcmp_until.obj" \
	"$(INTDIR)\win32_posix.obj" \
	"$(INTDIR)\get_exec_path.obj"

"..\src\condor_util_lib\condor_util.lib" : "$(OUTDIR)" $(DEF_FILE) $(LIB32_OBJS)
    $(LIB32) @<<
  $(LIB32_FLAGS) $(DEF_FLAGS) $(LIB32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_util_lib.dep")
!INCLUDE "condor_util_lib.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_util_lib.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_util_lib - Win32 Debug" || "$(CFG)" == "condor_util_lib - Win32 Release"
SOURCE=..\src\condor_util_lib\basename.c

"$(INTDIR)\basename.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\blankline.c

"$(INTDIR)\blankline.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\chomp.c

"$(INTDIR)\chomp.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\ckpt_name.c

"$(INTDIR)\ckpt_name.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\condor_common_c.C

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yc"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\condor_common_c.obj"	"$(INTDIR)\condor_common_c.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yc"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\condor_common_c.obj"	"$(INTDIR)\condor_common_c.pch" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\condor_perms.c

"$(INTDIR)\condor_perms.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\condor_snutils.c

"$(INTDIR)\condor_snutils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\condor_universe.c

"$(INTDIR)\condor_universe.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\copy_file.c

"$(INTDIR)\copy_file.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\cronos.c

"$(INTDIR)\cronos.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\d_format_time.c

"$(INTDIR)\d_format_time.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\dprintf.c

"$(INTDIR)\dprintf.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\dprintf_common.c

"$(INTDIR)\dprintf_common.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\dprintf_config.c

"$(INTDIR)\dprintf_config.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\email.c

"$(INTDIR)\email.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\email_file.c

"$(INTDIR)\email_file.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\escapes.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\escapes.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fp"$(INTDIR)\condor_common_c.pch" /Yu"condor_common.h" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\escapes.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\except.c

"$(INTDIR)\except.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\fdprintf.c

"$(INTDIR)\fdprintf.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\filename_tools.c

"$(INTDIR)\filename_tools.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\get_exec_path.c

"$(INTDIR)\get_exec_path.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\get_port_range.c

"$(INTDIR)\get_port_range.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\get_random_num.c

"$(INTDIR)\get_random_num.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\globus_utils.c

"$(INTDIR)\globus_utils.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\internet.c

"$(INTDIR)\internet.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\lock_file.WIN32.c

"$(INTDIR)\lock_file.WIN32.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\ltrunc.c

"$(INTDIR)\ltrunc.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\match_prefix.c

"$(INTDIR)\match_prefix.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\mkargv.c

"$(INTDIR)\mkargv.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\nullfile.c

"$(INTDIR)\nullfile.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\proc_id.c

"$(INTDIR)\proc_id.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\rotate_file.c

"$(INTDIR)\rotate_file.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_util_lib\setsyscalls.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\setsyscalls.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\setsyscalls.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\signames.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\signames.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\signames.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\condor_util_lib\strcmp_until.c

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

CPP_SWITCHES=/nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "_DEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\strcmp_until.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

CPP_SWITCHES=/nologo /MT /W3 /GX /Z7 /O1 /I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /D "WIN32" /D "NDEBUG" /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 

"$(INTDIR)\strcmp_until.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) @<<
  $(CPP_SWITCHES) $(SOURCE)
<<


!ENDIF 

SOURCE=..\src\h\syscall_numbers.tmpl

!IF  "$(CFG)" == "condor_util_lib - Win32 Debug"

InputDir=..\src\h
InputPath=..\src\h\syscall_numbers.tmpl

"..\src\h\syscall_numbers.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	$(InputDir)\..\..\NTconfig\awk.exe -f $(InputDir)\awk_prog.include_file $(InputDir)\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h
<< 
	

!ELSEIF  "$(CFG)" == "condor_util_lib - Win32 Release"

InputDir=..\src\h
InputPath=..\src\h\syscall_numbers.tmpl

"..\src\h\syscall_numbers.h" : $(SOURCE) "$(INTDIR)" "$(OUTDIR)"
	<<tempfile.bat 
	@echo off 
	$(InputDir)\..\..\NTconfig\awk.exe -f $(InputDir)\awk_prog.include_file $(InputDir)\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h
<< 
	

!ENDIF 

SOURCE=..\src\condor_util_lib\win32_posix.c

"$(INTDIR)\win32_posix.obj" : $(SOURCE) "$(INTDIR)" "$(INTDIR)\condor_common_c.pch"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

